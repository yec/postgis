<?php
/**
 * @file
 * GeoServer layer type for PostGIS fields.
 */

// autoloading is a mess
require_once(dirname(__FILE__) . "/geoserver_layer_type_postgis.inc");

/**
 * Define the Ctools plugin options.
 */
$plugin = array(
  'title' => t('PostGIS Field'),
  'description' => t('An entity containing a PostGIS field.'),
  'layer_type' => array(
    'file' => 'geoserver_layer_type_postgis_field.inc',
    'class' => 'geoserver_layer_type_postgis_field',
    'parent' => 'geoserver_layer_type',
  ),
);


/**
 * GeoServer PostGIS layer class.
 */
class geoserver_layer_type_postgis_field extends geoserver_layer_type_postgis {

  function save() {
    $this->create_view();
    return parent::save();
  }

  function create_view() {

    db_query($this->data['options']['sql']);
    $primary_keys = array('table_schema', 'table_name', 'pk_column');
    $sql_name = $this->data['options']['sql_name'];
    $schema = $this->get_schema($sql_name);

    // Insert/Update rercords in geoserver_pk_metadata_table.
    foreach (array('id', 'delta') as $idx => $column) {
      $record = array(
        'table_schema' => $schema,
        'table_name' => $sql_name,
        'pk_column' => $column,
        'pk_column_idx' => $idx
      );
      db_select('geoserver_pk_metadata_table', 'gs_pk')
          ->fields('gs_pk')
          ->condition('table_schema', $record['table_schema'])
          ->condition('table_name', $record['table_name'])
          ->condition('pk_column', $record['pk_column'])
          ->execute()
          ->fetchAll() ?
        drupal_write_record('geoserver_pk_metadata_table', $record, $primary_keys) :
        drupal_write_record('geoserver_pk_metadata_table', $record);
    }
  }

  function delete() {

    $sql_name = $this->data['options']['sql_name'];
    $schema = $this->get_schema($sql_name);
    $name = $schema . '.' . $sql_name;

    db_delete('geoserver_pk_metadata_table')
      ->condition('table_schema', $schema)
      ->condition('table_name', $sql_name)
      ->execute();

    db_query('DROP VIEW IF EXISTS ' . db_escape_table($name));
    return parent::delete();
  }

  /**
   * ???
   */
  private function get_schema(&$name) {
    if ($pos = strpos($name, '.')) {
      $name = substr($name, $pos) . $name;
      $schema = substr($name, 0, $pos);
    } else {
      $prefix = Database::getConnection()->tablePrefix();
      if ($pos = strpos($prefix, '.')) {
        $name = substr($prefix, $pos) . $name;
        $schema = substr($prefix, 0, $pos);
      } else {
        $name = $prefix . $name;
        $schema = 'public';
      }
    }
    return $schema;
  }


  /**
   * Export to remote feature type definition.
   */
  function to_feature_type() {
    $feature_type = parent::to_feature_type();
    $feature_type['featureType']['nativeName'] = isset($options['sql_name']) ? $options['sql_name'] : '';
    return $feature_type;
  }

  /**
   * Load field configuration.
   *
   * TODO: move stuff to baseclass in geoserver module
   */
  function load_source($source) {

    $prefix       = Database::getConnection()->tablePrefix();
    $source_array = explode('.', $source);
    $entity_type  = $source_array[0];
    $bundle_name  = $source_array[1];
    $field_name   = $source_array[2];
    $sql_name     = $prefix . 'geoserver_' . $this->name . '_view';
    $sql          = $this->get_sql($entity_type, $bundle_name, $field_name, $sql_name);
    $field        = field_info_field($field_name);
    $bbox         = $this->max_boundingbox_for_srid($field['settings']['srid']);

    $this->data['options']['srid'] = $field['settings']['srid'];
    $this->data['options']['sql_name'] = $sql_name;
    $this->data['options']['sql'] = $sql;
    $this->data['options']['bounding_box']['minx'] = $bbox[0];
    $this->data['options']['bounding_box']['miny'] = $bbox[1];
    $this->data['options']['bounding_box']['maxx'] = $bbox[2];
    $this->data['options']['bounding_box']['maxy'] = $bbox[3];
  }

  /**
   * Get layer SQL definition.
   */
  protected function get_sql($entity_type, $bundle_name, $field_name, $name) {
    $sql = array(
      'select' => array("DROP VIEW IF EXISTS $name;
CREATE VIEW $name AS SELECT $entity_type.nid as id, $entity_type.nid, $entity_type.title as name, field_data_$field_name.delta, field_data_$field_name.{$field_name}_geometry"),
      'from' => array("FROM $prefix$entity_type LEFT JOIN field_data_$field_name ON node.nid = field_data_$field_name.entity_id"),
      'where' => array("WHERE $entity_type.type = '$bundle_name' and $entity_type.status = 1 and field_data_$field_name.deleted = 0"),
      'rules' => array(
        "CREATE OR REPLACE RULE ${name}_update AS ON UPDATE TO $name DO INSTEAD
  UPDATE field_data_$field_name SET ${field_name}_geometry = NEW.${field_name}_geometry WHERE entity_id = NEW.id;",
        "CREATE OR REPLACE RULE ${name}_insert AS ON INSERT TO ${name} DO INSTEAD (
    INSERT INTO field_data_$field_name VALUES ('node', '$bundle_name', 0, nextval('node_nid_seq'), currval('node_nid_seq'), 'und', 0, NEW.${field_name}_geometry);
    INSERT INTO node VALUES (currval('node_nid_seq'), currval('node_nid_seq'), '$bundle_name', 'und', '', 0, 1, extract(epoch from now())::int, extract(epoch from now())::int, 0, 0, 0, 0, 0);
    INSERT INTO node_revision VALUES (currval('node_nid_seq'), currval('node_nid_seq'), 0, '', '', extract(epoch from now())::int, 1, 0, 0, 0);
  );",
        "CREATE OR REPLACE RULE ${name}_delete AS ON DELETE TO ${name} DO INSTEAD
  DELETE FROM field_data_$field_name  WHERE entity_id = OLD.id and delta = OLD.delta;",
      ),
    );

    // Join additional fields with a 1:1 mapping to this bundle.
    $fields = field_info_instances($entity_type, $bundle_name);
    foreach ($fields as $name => $field) {
      $field = field_info_field($name);
      if ($name != $field_name && $field['cardinality'] === '1') {
        foreach ($field['storage']['details']['sql']['FIELD_LOAD_CURRENT'] as $table => $table_fields) {
          foreach ($table_fields as $table_field) {
            $sql['select'][] = $table_field;
          }
          $sql['from'][] = "LEFT JOIN $prefix$table ON $entity_type.nid = $table.entity_id";
        }
      }
    }
    return implode(', ', $sql['select']) . "\n" . implode("\n", $sql['from']) . "\n" . implode("\n", $sql['where']) . ";\n" . implode("\n", $sql['rules']);
  }

  /**
   * Options form which generates layers
   */
  function options_form($form = array()) {
    if (empty($form)) {
      $formPart1 = array();
      $formPart1['sql_name'] = array(
          '#type' => 'textfield',
          '#title' => t('SQL View Name'),
          '#default_value' => isset($this->data['options']['sql_name']) ?
                  $this->data['options']['sql_name'] : '',
      );

      $formPart1['sql'] = array(
          '#type' => 'textarea',
          '#title' => t('SQL'),
          '#default_value' => isset($this->data['options']['sql']) ?
                  $this->data['options']['sql'] : '',
      );
      return array_merge($formPart1, parent::options_form($form));
    } else {
      $form = parent::options_form($form);
      $form['sql_name']['#value'] = $this->data['options']['sql_name'];
      $form['sql']['#value'] = $this->data['options']['sql'];
      return $this->options_form_ajax_variant($form);
    }
  }

  protected function options_form_ajax_fieldnames() {
    $fieldNames = parent::options_form_ajax_fieldnames();
    $fieldNames[] = 'sql';
    $fieldNames[] = 'sql_name';
    return $fieldNames;
  } 

  /**
   * @return array Mapping with all fields that are provided by the postgis module (fields that contain spatial data).
   */
  static function get_sources() {
    $sources = array('');

    $entityTypes = entity_get_info();
    foreach(field_info_fields() as $fieldName => $field){
      // Offer all fields of the postgis module
      if ($field['type'] === 'postgis') {
        foreach($field['bundles'] as $entityType => $contentTypes){
          foreach($contentTypes as $contentType) {
            $key = $entityType.'.'.$contentType.'.'.$field['field_name'];
            $value = $entityTypes[$entityType]['bundles'][$contentType]['label'].' ('.$field['field_name'].')';
            $sources[$entityTypes[$entityType]['label']][$key] = $value;
          }
        }
      }
    }
    return $sources;
  }


}

// vim: tabstop=2 shiftwidth=2
