<?php

/**
 * @file
 * Field for geospatial data using the Field API.
 * Implements field widgets.
 */

/**
 * Implements hook_field_widget_info().
 */
function postgis_field_widget_info() {

  $widgets = array(
    'wkt' => array(
      'label' => t('Textfield'),
      'field types' => array('postgis'),
    ),
  );

  if (module_exists('openlayers')) {
    $widgets['openlayers'] = array(
      'label' => t('OpenLayers Map'),
      'field types' => array('postgis'),
      'settings' => array(
        'map'      => 'postgis_widget_map',
        'behavior' => 'openlayers_behavior_drawfeatures',
      ),
      'behaviors' => array(
        'multiple values' => FIELD_BEHAVIOR_CUSTOM,
        'default value'   => FIELD_BEHAVIOR_NONE,
      ),
    );
  }

  return $widgets;
}

/**
 * Implements hook_field_widget_settings_form().
 */
function postgis_field_widget_settings_form($field, $instance) {
  $widget = $instance['widget'];
  $settings = $widget['settings'];

  $form = array();

  if ($widget['type'] == 'openlayers') {

    $maps = openlayers_map_options();

    $behaviors = openlayers_behaviors();
    foreach($behaviors as $index => $behavior) {
      $behaviors[$index] = $behavior['title'];
    }

    $map      = isset($settings['map']) ? $settings['map'] : 'postgis_widget_map';
    $behavior = isset($settings['behavior']) ? $settings['behavior'] : 'openlayers_behavior_drawfeatures';

    $form['map'] = array(
      '#type' => 'select',
      '#title' => t('OpenLayers Map'),
      '#default_value' => $map,
      '#options' => $maps,
      '#description' => t('Select which OpenLayers map you would like to use.'),
    );

    $form['behavior'] = array(
      '#type' => 'select',
      '#title' => t('OpenLayers Behavior'),
      '#default_value' => $behavior,
      '#options' => $behaviors,
      '#description' => t('Select an OpenLayers behavior which handles drawing of features.'),
    );

  }

  return $form;
}


/**
 * Implements hook_field_widget_form().
 */
function postgis_field_widget_form(&$form, &$form_state, $field, $instance, $langcode, $items, $delta, $element) {
  $widget = $instance['widget'];
  $settings = $widget['settings'];

  $value = '';
  if (isset($items[0]['geom'])) {
    $value = postgis_convert_output($field['settings']['geo_type'], $instance['widget']['type'], $items);
  }

  $element += array(
    '#delta' => $delta,
  );

  if (module_exists('openlayers') && $instance['widget']['type'] == 'openlayers') {

    switch ($field['settings']['geo_type']) {
      case 'point':
      case 'multipoint':
        $types = array('point');
        break;
      case 'linestring':
      case 'multilinestring':
        $types = array('path');
        break;
      case 'polygon':
      case 'multipolygon':
        $types = array('polygon');
        break;
      case 'geometrycollection':
        $types = array('point', 'path', 'polygon');
        break;
      default:
        $types = array();
    }

    $map      = openlayers_map_load($settings['map']);
    $behavior = isset($map->data['behaviors'][$settings['behavior']]) ?
        $map->data['behaviors'][$settings['behavior']] : array();

    $map->data['behaviors'][$settings['behavior']] = array_merge($behavior, array(
      'element_id'    => $field['field_name'] . '_wkt',
      'srid'          => $field['settings']['srid'],
      'feature_types' => $types,
      'feature_limit' => $field['cardinality'],
    ));

    $element['map'] = array(
      '#markup' => '<div class="form-item" style="display:block">' . openlayers_render_map($map->data) . '</div>',
    );

    $element['wkt'] = array(
      '#type' => 'textarea',
      '#attributes' => array('id' => $field['field_name'] . '_wkt', 'style' => 'display: none;'),
      '#default_value' => $value,
      '#required' => $element['#required'],
      '#resizable' => FALSE,
    );

  }
  else {
    $element['wkt'] = array(
      '#title' => t('Well Known Text'),
      '#type' => 'textfield',
      '#default_value' => $value,
      '#required' => $element['#required'],
    );
  }
  return $element;
}
