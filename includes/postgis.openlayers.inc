<?php

/**
 * @file
 * Provides OpenLayers maps and layers for widget and formatter.
 */

/**
 * Implements hook_openlayers_maps().
 */
function postgis_openlayers_maps() {
  $widget = new stdClass();
  $widget->api_version = 1;
  $widget->name = 'postgis_widget_map';
  $widget->title = t('PostGIS Widget Map');
  $widget->description = t('A map used for PostGIS input (for feature editing only)');
  $widget->data = array(
    'projection' => '900913',
    'width' => '600px',
    'default_layer' => 'osm_mapnik',
    'height' => '400px',
    'center' => array(
      'initial' => array(
        'centerpoint' => '0,0',
        'zoom' => '2',
      ),
    ),
    'image_path' => drupal_get_path('module', 'openlayers') . '/themes/default_dark/img/',
    'css_path' => drupal_get_path('module', 'openlayers') . '/themes/default_dark/style.css',
    'options' => array(
      'maxExtent' => openlayers_get_extent('900913'),
    ),
    'behaviors' => array(
      'openlayers_behavior_navigation' => array(),
      'openlayers_behavior_panzoombar' => array(),
    ),
    'layers' => array(
      'osm_mapnik' => 'osm_mapnik',
    ),
  );

  $formatter = new stdClass();
  $formatter->api_version = 1;
  $formatter->name = 'postgis_formatter_map';
  $formatter->title = t('PostGIS Formatter Map');
  $formatter->description = t('A map used for PostGIS output (for feature display only)');
  $formatter->data = array(
    'projection' => '900913',
    'width' => '600px',
    'default_layer' => 'osm_mapnik',
    'height' => '400px',
    'center' => array(
      'initial' => array(
        'centerpoint' => '0,0',
        'zoom' => '2',
      ),
    ),
    'image_path' => drupal_get_path('module', 'openlayers') . '/themes/default_dark/img/',
    'css_path' => drupal_get_path('module', 'openlayers') . '/themes/default_dark/style.css',
    'options' => array(
      'maxExtent' => openlayers_get_extent('900913'),
    ),
    'behaviors' => array(
      'openlayers_behavior_keyboarddefaults' => array(),
      'openlayers_behavior_navigation' => array(),
      'openlayers_behavior_panzoombar' => array(),
      'openlayers_behavior_zoomtolayer' => array(
        'zoomtolayer' => array(
          'postgis_formatter_layer' => 'postgis_formatter_layer',
          'osm_mapnik' => 0,
        ),
        'point_zoom_level' => '10',
        'zoomtolayer_scale' => '1',
      ),
    ),
    'layers' => array(
      'osm_mapnik' => 'osm_mapnik',
      'postgis_formatter_layer' => 'postgis_formatter_layer',
    ),
  );

  return array(
    $widget->name    => $widget,
    $formatter->name => $formatter,
  );
}

/**
 * Implements hook_openlayers_layers().
 */
function postgis_openlayers_layers() {
  $layers = array();
  $layer = new stdClass();
  $layer->api_version = 1;
  $layer->name = 'postgis_formatter_layer';
  $layer->title = 'PostGIS output layer';
  $layer->description = '';
  $layer->data = array(
    'layer_type' => 'openlayers_layer_type_raw',
    'projection' => array('900913'),
    'features' => array(),
  );
  $layers[$layer->name] = $layer;
  return $layers;
}

