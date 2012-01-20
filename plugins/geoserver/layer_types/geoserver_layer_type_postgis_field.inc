<?php
/**
 * @file
 * GeoServer layer type for PostGIS fields.
 */

/**
 * Define the Ctools plugin options.
 */
$plugin = array(
  'title' => t('PostGIS'),
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
class geoserver_layer_type_postgis_field extends geoserver_layer_type {

  /**
   * Constructor.
   */
  function __construct($layer = array()) {
    parent::__construct($layer);
    $this->datastore = variable_get('geoserver_postgis_datastore', '');
  }
  
  function from_remote($remote) {
    
    foreach ($remote->featureType->metadata->entry as $entry) {
      if (isset($entry->virtualTable)) {
        $virtualTable = $entry->virtualTable;
        break;
      }
    }
    
    $remote_layer = new stdClass();
    $remote_layer->name = $remote->featureType->name;
    $remote_layer->title = $remote->featureType->title;
    $remote_layer->description = $this->description;
    $remote_layer->data = array(
      'field' => $this->data['field'],
      'srid' => drupal_substr($remote->featureType->srs, 5),
      'geometry_name' =>  $virtualTable->geometry->name,
      'geometry_type' =>  $virtualTable->geometry->type,
      'bounding_box'=> array(
        'minx' => $remote->featureType->nativeBoundingBox->minx,
        'miny' => $remote->featureType->nativeBoundingBox->miny,
        'maxx' => $remote->featureType->nativeBoundingBox->maxx,
        'maxy' => $remote->featureType->nativeBoundingBox->maxy,
      ),
      'sql' => $virtualTable->sql,
      'layer_type' => $this->data['layer_type'],
    );
    return new geoserver_layer_type_postgis_field($remote_layer);
  }
  
  function to_remote() {
    $bounding_box = isset($this->data['bounding_box']) ? $this->data['bounding_box'] : '';
    return array(
      'featureType' => array(
        'name'       => $this->name,
        'nativeName' => $this->name,
        'namespace'  => array('name' => variable_get('geoserver_workspace', '')),
        'title'      => $this->title,
        'nativeCRS'  => isset($this->data['srid']) ? 'EPSG:' . $this->data['srid'] : '',
        'srs'        => isset($this->data['srid']) ? 'EPSG:' . $this->data['srid'] : '',
        'nativeBoundingBox' => array(
          'minx' => isset($bounding_box['minx']) ? $bounding_box['minx'] : '',
          'miny' => isset($bounding_box['miny']) ? $bounding_box['miny'] : '',
          'maxx' => isset($bounding_box['maxx']) ? $bounding_box['maxx'] : '',
          'maxy' => isset($bounding_box['maxy']) ? $bounding_box['maxy'] : '',
        ),
        'latLonBoundingBox' => array(
          'minx' => isset($bounding_box['minx']) ? $bounding_box['minx'] : '',
          'miny' => isset($bounding_box['miny']) ? $bounding_box['miny'] : '',
          'maxx' => isset($bounding_box['maxx']) ? $bounding_box['maxx'] : '',
          'maxy' => isset($bounding_box['maxy']) ? $bounding_box['maxy'] : '',
        ),
        'projectionPolicy' => 'FORCE_DECLARED',
        'enabled' => TRUE,
        'metadata' => array(
          'entry' => array(
            array(
              '@key' => 'cachingEnabled', 
              '$' => 'false',
            ),
            array(
              '@key' => 'JDBC_VIRTUAL_TABLE',
              'virtualTable' => array(
                'name' => $this->name,
                'sql' => isset($this->data['sql']) ? $this->data['sql'] : '',
                'keyColumn' => 'id',
                'geometry' => array(
                  'name' => isset($this->data['geometry_name']) ? $this->data['geometry_name'] : '',
                  'type' => isset($this->data['geometry_type']) ? $this->data['geometry_type'] : '',
                  'srid' => isset($this->data['srid']) ? $this->data['srid'] : '',
                ),
              ),
            ),
          ),
        ),
        'store' => array(
          '@class' => 'dataStore',
          'name' => $this->datastore,
        ),
        'maxFeatures' => 0,
        'numDecimals' => 0,
        'attributes' => array(),
      ),
    );
  }
  
  function load_field($entity_type, $bundle_name, $field_name) {
    
    // Map geometry types from PostGIS to GeoServer.
    $mapping = array(
      'point' => 'Point',
      'linestring' => 'LineString',
      'polygon' => 'Polygon',
      'multipoint' => 'MultiPoint',
      'multilinestring' => 'MultiLineString',
      'multipolygon'  => 'MultiPolygon',
      'geometrycollection' => 'GeometryCollection',
    );
    $field = field_info_field($field_name);
    $bbox = postgis_field_extent($field);
    
    $this->data['srid'] = $field['settings']['srid'];
    $this->data['geometry_name'] = $field_name . '_geometry';
    $this->data['geometry_type'] = $mapping[$field['settings']['type']];
    $this->data['bounding_box']['minx'] = $bbox->minx;
    $this->data['bounding_box']['miny'] = $bbox->miny;
    $this->data['bounding_box']['maxx'] = $bbox->maxy;
    $this->data['bounding_box']['maxy'] = $bbox->maxx;
    $this->data['sql'] = $this->get_sql($entity_type, $bundle_name, $field_name);
  }
  
  private function get_sql($entity_type, $bundle_name, $field_name) {

    $connection = Database::getConnection();
    $prefix = $connection->tablePrefix();

    $sql = array(
        'fields' => array("SELECT $entity_type.nid as id, $entity_type.title"),
        'tables' => array("\nFROM $prefix$entity_type"),
        'where' => array("\nWHERE $entity_type.type = '$bundle_name' and $entity_type.status = 1"),
    );

    $fields = field_info_instances($entity_type, $bundle_name);

    foreach ($fields as $field_name => $field) {

      $field = field_info_field($field_name);

      if ($field['cardinality'] === '1') {
        foreach ($field['storage']['details']['sql']['FIELD_LOAD_CURRENT'] as $table => $table_fields) {
          foreach ($table_fields as $table_field) {
            $sql['fields'][] = $table_field;
          }
          $sql['tables'][] = "\nLEFT JOIN $prefix$table ON $entity_type.nid = $table.entity_id";
        }
      }
    }
    return implode(', ', $sql['fields']) . ' ' . implode(' ', $sql['tables']) . ' ' . implode(' ', $sql['where']);
  }

  /**
   * Options form which generates layers
   */
  function options_form() {
    
    $field_options = array('');
    $fields = field_info_instances();
    
    foreach ($fields as $entity_type => $bundles) {
      
      $info = entity_get_info($entity_type);
      
      foreach ($bundles as $bundle_name => $fields) {
        
        foreach ($fields as $field_name => $field) {
          
          $field = field_info_field($field_name);
          
          if ($field['type'] === 'postgis') {
            $key = $entity_type . '.' . $bundle_name . '.' . $field_name;
            $value = $info['bundles'][$bundle_name]['label'] . ' (' . $field_name . ')';
            $field_options[$info['label']][$key] = $value;
            break;
          }
        }
      }
    }
    
    return array(
      'field' => array(
        '#type' => 'select',
        '#title' => t('Field'),
        '#options' => $field_options,
        '#default_value' => isset($this->data['field']) ? $this->data['field'] : '',
        '#ajax' => array(
          'callback' => 'postgis_geoserver_layer_type_form',
          'wrapper' => 'geoserver_layer_type_postgis_field',
        ),
      ),
      'srid' => array(
        '#type' => 'textfield',
        '#title' => t('Projection'),
        '#prefix' => '<div id="geoserver_layer_type_postgis_field">',
        '#default_value' => isset($this->data['srid']) ? $this->data['srid'] : '',
      ),
      'geometry_name' => array(
        '#type' => 'textfield',
        '#title' => t('Geometry name'),
        '#default_value' => isset($this->data['geometry_name']) ? $this->data['geometry_name'] : '',
      ),
      'geometry_type' => array(
        '#type' => 'select',
        '#title' => t('Geometry type'),
        '#default_value' => isset($this->data['geometry_type']) ? $this->data['geometry_type'] : '',
        '#options' => array(
          'Geometry' => 'Geometry',
          'GeometryCollection' => 'GeometryCollection',
          'Point' => 'Point',
          'MultiPoint' => 'MultiPoint',
          'LineString' => 'LineString',
          'MultiLineString' => 'MultiLineString',
          'Polygon' => 'Polygon',
          'MultiPolygon' => 'MultiPolygon',
        ),
      ),
      'bounding_box' => array(
        '#type' => 'fieldset',
        '#title' => t('Bounding Box'),
        '#collapsible' => TRUE,
        '#collapsed' => TRUE,
        'minx' => array(
          '#type' => 'textfield',
          '#title' => t('Min X'),
          '#default_value' => isset($this->data['bounding_box']['minx']) ? $this->data['bounding_box']['minx'] : '',
        ),
        'miny' => array(
          '#type' => 'textfield',
          '#title' => t('Min Y'),
          '#default_value' => isset($this->data['bounding_box']['miny']) ? $this->data['bounding_box']['miny'] : '',
        ),
        'maxx' => array(
          '#type' => 'textfield',
          '#title' => t('Max X'),
          '#default_value' => isset($this->data['bounding_box']['maxx']) ? $this->data['bounding_box']['maxx'] : '',
        ),
        'maxy' => array(
          '#type' => 'textfield',
          '#title' => t('Max Y'),
          '#default_value' => isset($this->data['bounding_box']['maxy']) ? $this->data['bounding_box']['maxy'] : '',
        ),
      ),
      'sql' => array(
        '#type' => 'textarea',
        '#title' => t('SQL'),
        '#description' => t('SQL ...'),
        '#default_value' => isset($this->data['sql']) ? $this->data['sql'] : '',
        '#suffix' => '</div>',
      ),
      'layer_type' => array(
        '#type' => 'hidden',
        '#value' => 'geoserver_layer_type_postgis_field'
      )
    );
  }
  
  function settings_form() {
    
    $datastores = array('');
    $workspace  = variable_get('geoserver_workspace', '');
    
    $result = geoserver_get('rest/workspaces/' . $workspace . '/datastores.json');
    if ($result->code == 200) {
      foreach ($result->data->dataStores->dataStore as $datastore) {
        $datastores[$datastore->name] = $datastore->name;
      }
    }
    
    return array(
      'geoserver_postgis_datastore' => array(
        '#type' => 'select',
        '#title' => t('Datastore'),
        '#options' => $datastores,
        '#description' => t('Select the PostGIS datastore where Drupal stores its data. This one needs to be created manually for now.'),
        '#default_value' => variable_get('geoserver_postgis_datastore', ''),
        '#suffix' => '</div>',
      )
    );
  }
}
