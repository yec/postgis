<?php
/**
 * @file
 * GeoServer layer type for PostGIS tables layers.
 */

// autoloading is a mess
require_once(dirname(__FILE__) . "/geoserver_layer_type_postgis.inc");

/**
 * Define the Ctools plugin options.
 */
$plugin = array(
  'title' => t('PostGIS Table'),
  'description' => t('Data from an PostGIS Table.'),
  'layer_type' => array(
    'file' => 'geoserver_layer_type_postgis_table.inc',
    'class' => 'geoserver_layer_type_postgis_table',
    'parent' => 'geoserver_layer_type',
  ),
);


class geoserver_layer_type_postgis_table extends geoserver_layer_type_postgis {

  /** 
   * returns a list of all tables with postgis geometries except tables
   * of postgis_field nodes.
   *
   * this will propably have problems when tables contain more than one geometry
   */
  static function get_sources() {
    $sources = array();

    $stmt = db_query("select distinct quote_ident(table_schema) || '.' || quote_ident(table_name) as source, table_schema, table_name ".
        " from information_schema.columns " .
        " where udt_name='geometry' ".
        "   and table_name not like 'field_data_%' and table_name not like 'field_revision_%'".
        " order by table_schema, table_name");

    if ($stmt) {
      $row = $stmt->fetchAssoc();
      while ($row) {
        $sources[$row['source']] = $row['source'];
        $row = $stmt->fetchAssoc();
      }
    }
    return $sources;
  }
}

// vim: tabstop=2 shiftwidth=2
