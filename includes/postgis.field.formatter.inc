<?php

/**
 * @file
 * Field for geospatial data using the Field API. Implements field formatter.
 */

/**
 * Implements hook_field_formatter_info().
 */
function postgis_field_formatter_info() {

  $formatters = array(
    'wkt' => array(
      'label' => t('Well Known Text (WKT)'),
      'field types' => array('postgis'),
    ),
    'measurement' => array(
      'label' => t('Measurement'),
      'field types' => array('postgis'),
      'settings' => array('type' => 'length'),
    ),
  );

  if (module_exists('openlayers')) {
    $formatters['openlayers'] = array(
      'label' => t('OpenLayers Map'),
      'field types' => array('postgis'),
      'settings' => array('map' => 'postgis_formatter_map'),
    );
  }

  return $formatters;
}

/**
 * Implements hook_field_formatter_settings_form().
 */
function postgis_field_formatter_settings_form($field, $instance, $view_mode, $form, &$form_state) {
  $display = $instance['display'][$view_mode];
  $settings = $display['settings'];

  if ($display['type'] == 'openlayers' && module_exists('openlayers')) {

    // OpenLayers maps filtered to those which have the postgis_formatter_layer
    $maps = openlayers_maps();
    $maps_options = array();
    foreach ($maps as $map) {
      if (in_array('postgis_formatter_layer', $map->data['layers'])) {
        $maps_options[$map->name] = $map->title;
      }
    }

    if (empty($maps)) {
      form_set_error('map', t('Error: You have no compatible OpenLayers maps. Make sure that at least one map has the PostGIS formatter layer enabled.'));
    }

    $element['map'] = array(
      '#title' => t('OpenLayers map'),
      '#type' => 'select',
      '#default_value' => $settings['map'] ? $settings['map'] : 'postgis_formatter_map',
      '#required' => TRUE,
      '#options' => $maps_options,
      '#description' => t('Select which OpenLayers map you would like to use. Only maps which have the PostGIS formatter layer may be selected.'),
    );
  }

  return $element;
}

/**
 * Implements hook_field_formatter_settings_summary().
 */
function postgis_field_formatter_settings_summary($field, $instance, $view_mode) {
  $display = $instance['display'][$view_mode];
  $settings = $display['settings'];

  $summary = array();

  if ($display['type'] == 'openlayers' && isset($settings['map'])) {
    $maps = openlayers_map_options();
    $summary[] = t('Openlayers Map: @data', array('@data' => $maps[$settings['map']]));
  }

  return implode('<br />', $summary);
}

/**
 * Implements hook_field_formatter_view().
 */
function postgis_field_formatter_view($entity_type, $entity, $field, $instance, $langcode, $items, $display) {

  $element = array();

  $set = new PostgisGeometrySet($field['settings']['type'], $field['settings']['srid']);
  $set->fromGeometry($items);
  $wkt = $set->getText();

  switch ($display['type']) {

    case 'wkt':
      $element[]['#markup'] = $wkt;
      break;

    case 'measurement':
      // TODO: make measurement configurable
      $element[]['#markup'] = $set->length('km') . ' ' . t('km');
      break;

    case 'openlayers':

      $map_name = $display['settings']['map'] ? $display['settings']['map'] : 'postgis_formatter_map';
      $map = openlayers_map_load($map_name);
      $map = openlayers_build_map($map->data);

      $map['layers']['postgis_formatter_layer']['features'] = array(array(
        'wkt' => 'GEOMETRYCOLLECTION(' . $wkt . ')',
        'projection' => $field['settings']['srid'],
      ));

      // render map if no errors found
      if (empty($map['errors'])) {
        $js = array('openlayers' => array('maps' => array($map['id'] => $map)));
        drupal_add_js($js, 'setting');

        // push map through theme function
        $element[0]['#markup'] = theme('openlayers_map', array(
          'map' => $map,
          'map_name' => $map_name
        ));
      }

      break;
  }

  return $element;
}