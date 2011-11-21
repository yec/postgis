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
        'type' => 'point',
        'srid' => POSTGIS_DEFAULT_SRID,
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
    'multipolygon'  => t('Multi Polygon'),
    'geometrycollection' => t('Geometry Collection'),
  );

  $form['type'] = array(
    '#type' => 'select',
    '#title' => t('Geometry type'),
    '#options' => $types,
    '#default_value' => $field['settings']['type'],
    '#description' => t('Choose the geometry that best describes the information you are storing. "Point" is suitable for addresses and other fixed points, but you may also choose "Linestring" for lines or tracks, and "Polygon" for shapes and areas. If you have different types of geometry choose "Geometry Collection". To store multiple geometries of the same type in one table row the "Multi" geometry types and "Geometry Collection" are available.'),
    '#disabled' => $has_data,
  );

  $form['srid'] = array(
    '#type' => 'textfield',
    '#title' => t('SRID'),
    '#default_value' => $field['settings']['srid'],
    '#disabled' => $has_data,
  );

  // Let other modules extend the settings form.
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

  // Catch empty input.
  $wkt = isset($items['wkt']) ? $items['wkt'] : '';

  $set = new PostgisGeometrySet($field['settings']['type'], $field['settings']['srid'], $field['cardinality']);
  $set->fromText($wkt);

  $set->validate($field['field_name']);
}

/**
 * Implements hook_field_is_empty().
 */
function postgis_field_is_empty($item, $field) {

  $set = new PostgisGeometrySet($field['settings']['type'], $field['settings']['srid']);
  $set->fromText($item);

  return $set->isEmpty();
}


/**
 * Implements hook_field_presave().
 */
function postgis_field_presave($entity_type, $entity, $field, $instance, $langcode, &$items) {

  // Catch empty input.
  $wkt = count($items) > 0 ? $items[0] : '';

  $set = new PostgisGeometrySet($field['settings']['type'], $field['settings']['srid']);
  $set->fromText($wkt);

  $items = $set->getItems();
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
    $type = drupal_strtoupper($field['settings']['type']);
    $storage = field_sql_storage_field_storage_details($field);
    $connection = Database::getConnection();

    foreach ($storage['sql'] as $tables) {

      foreach ($tables as $table => $columns) {

        $prefix = $connection->tablePrefix($table);

        foreach ($columns as $field => $column) {

          db_drop_field($table, $column);

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

