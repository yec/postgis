<?php

/**
 * @file
 * PostGIS geometry classes.
 */

/**
 * Provides a set of postgis_geometry with same geometry type and projection.
 */
class postgis_geometry_set {

  private $geometries = array();
  private $cardinality, $type, $srid;

  /**
   * Create an empty geometry set.
   *
   * @param String $type        Geometry type
   * @param int    $srid        Spatial projection
   * @param int    $cardinality Number of geometries
   */
  function __construct($type, $srid, $cardinality = NULL) {
    $this->type = $type;
    $this->srid = $srid;
    $this->cardinality = $cardinality;
  }

  /**
   * Validate each geometry.
   *
   * @param String $field_name Name of geometry form field.
   */
  function validate($field_name) {

    foreach ($this->geometries as $geometry) {
      $result = $geometry->validate();
      if (!empty($result)) {
        form_set_error(check_plain($field_name), check_plain($result));
      }
    }
  }

  /**
   * Add geometries from text.
   *
   * @param array $items Geometries stored with key "wkt".
   */
  function fromText($items) {

    foreach ($items as $item) {
      $geometry = new postgis_geometry($this->type, $this->srid);
      $geometry->fromText($item['wkt']);
      $this->geometries[] = $geometry;
    }
  }

  /**
   * Add geometries from binary.
   *
   * @param array $items Geometries stored with key "geometry".
   */
  function fromGeometry($items) {

    foreach ($items as $item) {
      $geometry = new postgis_geometry($this->type, $this->srid);
      $geometry->fromGeometry($item['geometry']);
      $this->geometries[] = $geometry;
    }
  }

  /**
   * Get an array of geometries.
   * Text representation is stored with key "wkt" and 
   * binary representation is stored with key "geometry".
   *
   * @return array
   */
  function getItems() {

    $items = array();
    foreach ($this->geometries as $geometry) {
      $items[] = array(
        'wkt' => $geometry->getText(),
        'geometry' => $geometry->getGeometry(),
      );
    }
    return $items;
  }
}

/**
 * Simple class for a PostGIS geometry.
 */
class postgis_geometry {

  private $geometry, $wkt, $type, $srid;

  /**
   * Create an empty geometry.
   *
   * @param String $type Geometry type
   * @param int    $srid Spatial projection
   */
  function __construct($type, $srid) {
    $this->type = $type;
    $this->srid = $srid;
  }

  /**
   * Add geometry from text.
   *
   * @param String $wkt
   */
  function fromText($wkt) {

    // handle OpenLayers input
    if (strpos($wkt, 'GEOMETRYCOLLECTION') === 0 &&
        $this->type != 'GEOMETRYCOLLECTION') {
      // strip GEOMETRYCOLLECTION
      $wkt = drupal_substr($wkt, 19, -1);

      // handle MULTI geometries
      if (stripos($wkt, $this->type) === false &&
          stripos($this->type, 'MULTI') === 0) {

        // prepare for processing
        $wkt = explode('),', $wkt);

        // strip single geometry type
        foreach ($wkt as &$geo) {
          $geo = drupal_substr($geo, drupal_strlen($this->type)-4);
        }

        // prepend multi geometry type
        $wkt = $this->type . '((' . implode('),(', $wkt) . ')';

      }
    }
    $this->wkt = $wkt;
    $this->geometry = NULL;
  }

  /**
   * Add geometries from binary.
   *
   * @param <type> $geometry
   */
  function fromGeometry($geometry) {
    $this->geometry = $geometry;
    $this->wkt = NULL;
  }

  /**
   * Get text representation of geometry.
   *
   * @return String
   */
  function getText() {
    if (is_null($this->wkt)) {
      $this->wkt = db_query("SELECT ST_AsText(:geometry);",
          array(':geometry' => $this->geometry))->fetchField();
    }
    return $this->wkt;
  }

  /**
   * Get binary representation of geometry.
   *
   * @return <type>
   */
  function getGeometry() {
    if (is_null($this->geometry)) {
      $this->geometry = db_query("SELECT ST_GeomFromText(:wkt, :srid)",
        array(':wkt' => $this->wkt, ':srid' => $this->srid))->fetchField();
    }
    return $this->geometry;
  }

  /**
   * Validate geometry.
   *
   * @return String
   */
  function validate() {

    $geo = is_null($this->wkt) ? $this->geometry : $this->wkt;

    try {
      $result = db_query("SELECT ST_GeometryType(:geo), ST_IsValid(:geo), ST_IsValidReason(:geo) as reason",
          array(':geo' => $geo))->fetchAssoc();

      // return reason if geometry is not valid
      if (!$result['st_isvalid']) {
        return t('Not a valid geometry: @reason.', array('@reason' => $result['reason']));
      }

      $result_type = drupal_strtolower(drupal_substr($result['st_geometrytype'], 3));

      // return error if geometry type is unexpected
      if ($result_type != $this->type) {
        return t('Wrong geometry type. Got @result, was expecting @type.',
          array('@result' => drupal_strtoupper($result_type), '@type' => drupal_strtoupper($this->type)));
      }
    }
    catch (PDOException $e) {
      // TODO: catch only WKT parse errors
      return t('Unable to parse WKT.');
    }
  }

  /**
   * Check if geomety is empty.
   *
   * @return boolean
   */
  function isEmpty() {
    $geo = is_null($this->wkt) ? $this->geometry : $this->wkt;
    return empty($geo);
  }
}