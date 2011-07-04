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
      'settings' => array('map' => 'postgis_widget_map'),
      'behaviors' => array(
        'multiple values' => FIELD_BEHAVIOR_CUSTOM,
        'default value' => FIELD_BEHAVIOR_NONE,
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

    // OpenLayers maps filtered to those which have the draw features behavior
    $maps = openlayers_maps();
    $maps_options = array();
    foreach ($maps as $map) {
      if (array_key_exists('openlayers_behavior_drawfeatures', $map->data['behaviors'])) {
        $maps_options[$map->name] = $map->title;
      }
    }

    if (empty($maps)) {
      form_set_error('map', 'Error: You have no compatible OpenLayers maps. Make sure that at least one map has the draw features behavior enabled.');
    }

    $form['map'] = array(
      '#type' => 'select',
      '#title' => t('OpenLayers Map'),
      '#default_value' => isset($settings['map']) ? $settings['map'] : 'postgis_widget_map',
      '#options' => $maps_options,
      '#description' => t('Select which OpenLayers map you would like to use. Only maps which have the draw features behavior behavior may be selected.'),
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

    $map = openlayers_map_load($settings['map']);
    $map->data['behaviors']['openlayers_behavior_drawfeatures'] = array(
      'element_id' => $field['field_name'] . '_wkt',
      'feature_types' => $types,
      'feature_limit' => $field['cardinality'],
    );

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
