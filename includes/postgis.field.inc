<?php

/**
 * @file
 * Field for geospatial data using the Field API.
 */

/**
 * Implements hook_field_info().
 */
function postgis_field_info() {
  return array(
    'postgis' => array(
      'label' => t('Geospatial data'),
      'description' => t('This field stores geospatial data in the database.'),
      'settings' => array(
        'geo_type'     => 'point',
        'srid'         => POSTGIS_DEFAULT_SRID
      ),
      'instance_settings' => array(),
      'default_widget' => 'wkt',
      'default_formatter' => 'wkt',
    ),
  );
}

/**
 * Implements hook_field_settings_form().
 */
function postgis_field_settings_form($field, $instance, $has_data) {

  $types = array(
    'point' => t('Point'),
    'linestring' => t('Linestring'),
    'polygon' => t('Polygon'),
    'multipoint' => t('Multi Point'),
    'multilinestring' => t('Multi Linestring'),
    'multipolygon' => t('Multi Polygon'),
    'geometrycollection' => t('Geometry Collection'),
  );

  $form['geo_type'] = array(
    '#type' => 'select',
    '#title' => t('Geometry type'),
    '#options' => $types,
    '#default_value' => $field['settings']['geo_type'],
    '#description' => t('Choose the geometry that best describes the information you are storing. "Point" is suitable for addresses and other fixed points, but you may also choose "Linestring" for lines or tracks, and "Polygon" for shapes and areas. If you have different types of geometry choose "Geometry Collection". To store multiple geometries of the same type in one table row the "Multi" geometry types and "Geometry Collection" are available.'),
    '#disabled' => $has_data,
  );

  $form['srid'] = array(
    '#type' => 'textfield',
    '#title' => t('SRID'),
    '#default_value' => $field['settings']['srid'],
    '#disabled' => $has_data,
  );

  // let other modules extend the settings form
  $additions = module_invoke_all('postgis_field_settings_form', $field, $instance, $has_data);
  if (is_array($additions)) {
    $form = array_merge($form, $additions);
  }

  return $form;
}

/**
 * Implements hook_field_validate().
 */
function postgis_field_validate($entity_type, $entity, $field, $instance, $langcode, $items, &$errors) {

  postgis_parse_input($field, $instance, $items);

  foreach ($items as $delta => $item) {

    if (!postgis_field_is_empty($item, $field)) {

      try {
        $result = db_query("SELECT ST_GeometryType(:wkt), ST_IsValid(:wkt), ST_IsValidReason(:wkt) as reason",
            array(':wkt' => $item['wkt']))->fetchAssoc();

        // display reason if geometry is not valid
        if (!$result['st_isvalid']) {
          form_set_error($field['field_name'], t('Not a valid geometry: %reason.',
            array('%reason' => $result['reason'])));
        }

        $result_geo_type = drupal_strtolower(drupal_substr($result['st_geometrytype'], 3));

        // display error if geometry type is unexpected
        if ($result_geo_type != $field['settings']['geo_type']) {
          form_set_error($field['field_name'], t('Wrong geometry type. Got %result, was expecting %type.',
            array('%result' => $result_geo_type, '%type' => $field['settings']['geo_type'])));
        }
      }
      catch (PDOException $e) {
        form_set_error($field['field_name'], t('Unable to parse WKT.'));
      }
    }
  }
}

/**
 * Implements hook_field_is_empty().
 */
function postgis_field_is_empty($item, $field) {
  return empty($item['wkt']);
}


/**
 * Implements hook_field_presave().
 */
function postgis_field_presave($entity_type, $entity, $field, $instance, $langcode, &$items) {

  if (empty($items) || (is_array($items[0]) && key_exists('geom', $items[0]))) {
    return;
  }

  $items = $instance['widget']['type'] == 'openlayers' ? array('wkt' => $items[0]) : $items;
  postgis_parse_input($field, $instance, $items);

  foreach ($items as $delta => $item) {

    if (!postgis_field_is_empty($item['wkt'], $field)) {

      $items[$delta] = array();
      $items[$delta]['geom'] = postgis_wkt2geo($item['wkt'], $field['settings']['srid']);
    }
  }
}

/**
 * Implements hook_field_insert().
 */
function postgis_field_insert($entity_type, $entity, $field, $instance, $langcode, &$items) {
  postgis_field_diff($entity, $field);
}

/**
 * Implements hook_field_update().
 */
function postgis_field_update($entity_type, $entity, $field, $instance, $langcode, &$items) {
  postgis_field_diff($entity, $field);
}

/**
 * Implements hook_field_delete().
 */
function postgis_field_delete($entity_type, $entity, $field, $instance, $langcode, &$items) {
  postgis_field_diff($entity, $field);
}

/**
 * Calculate bounding box of changed area
 * and invoke hook_postgis_field_diff.
 *
 * @param <type> $entity
 * @param <type> $field
 */
function postgis_field_diff($entity, $field) {

  $fieldNew = $entity->$field['field_name'];

  $diff = db_query("SELECT Box2D( ST_Transform( SetSRID( ST_Extent(:geo), :epsg), :display_epsg));",
    array(
      ':display_epsg' => $field['settings']['display_srid'],
      ':epsg'         => $field['settings']['srid'],
      ':geo'          => $fieldNew[$entity->language][0]['geom'],
    ))->fetchField();

  $bbox = preg_split('/[\, ]/', substr($diff, 4, -1));

  // let other modules handle diffs
  module_invoke_all('postgis_field_diff', $field, $bbox);
}

/**
 * Implements hook_field_update_field().
 *
 * Keep table "geometry_columns" up to date.
 *
 */
function postgis_field_update_field($field, $prior_field, $has_data) {

  if (!$has_data && isset($field['settings']['srid'])) {

    $srid = $field['settings']['srid'];
    $type = drupal_strtoupper($field['settings']['geo_type']);
    $storage = field_sql_storage_field_storage_details($field);
    $connection = Database::getConnection();

    foreach ($storage['sql'] as $tables) {

      foreach ($tables as $table => $columns) {

        $prefix = $connection->tablePrefix($table);

        foreach ($columns as $field => $column) {

          db_query('ALTER TABLE ' . $prefix . $table . ' DROP COLUMN ' . $column);

          db_query(
            'SELECT AddGeometryColumn(:prefix, :table, :column, :srid, :type, 2)',
            array(
              ':prefix' => trim($prefix, '.'),
              ':table' => $table,
              ':column' => $column,
              ':srid' => $srid,
              ':type' => $type,
            )
          );

          db_query('CREATE INDEX ' . $table . '_' . $column . '_idx ON {' . $table . '} USING GIST ( ' . $column . ' )');
        }
      }
    }
  }
}
