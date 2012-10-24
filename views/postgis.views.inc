<?php

/**
 * @file
 * Hooks for Views integration.
 */

/**
 * Implements hook_views_data_alter().
 */
function postgis_views_data_alter(&$data) {
  foreach ($data as $table => $config) {
    if (isset($config['field_postgis'])) {
      $field_name = $config['field_postgis']['filter']['field_name'] . '_geometry';
      $data[$table][$field_name]['filter']['handler'] = 'postgis_views_handler_filter_geometry';
    }
  }
  dvm($data);
  return $data;
}