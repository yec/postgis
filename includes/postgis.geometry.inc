<?php

/**
 * @file
 * PostGIS geometry classes.
 */

class postgis_geometry_exception extends Exception
{
}

/**
 * Provides a set of postgis_geometry with same geometry type and projection.
 */
class postgis_geometry_set {

  private $geometries = array();
  private $cardinality, $type, $srid, $multi;

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
    $this->multi = stripos($type, 'multi') === 0;
    $this->cardinality = $cardinality;
  }

  /**
   * Validate each geometry.
   *
   * @param String $field_name Name of geometry form field.
   */
  function validate($field_name) {

    if ($this->count() > $this->cardinality) {
      form_set_error(check_plain($field_name), t('To many geometries. Got @result, was expecting @cardinality.',
          array('@result' => $this->count(), '@cardinality' => $this->cardinality)));
    }

    foreach ($this->geometries as $geometry) {
      $result = $geometry->validate();
      if (!empty($result)) {
        form_set_error(check_plain($field_name), check_plain($result));
      }
    }
  }

  function length($unit) {
  	$length = 0;
  	foreach ($this->geometries as $geometry) {
      $length += $geometry->length($unit);
    }
    return $length;
  }

  /**
   * Count geometries.
   *
   * @return int
   */
  function count() {
    return count($this->geometries);
  }

  /**
   * Add a geometrie to geometries.
   *
   * @param postgis_geometry $geometry
   */
  function add($geometry) {

    if (get_class($geometry) !== 'postgis_geometry') {
      throw new postgis_geometry_exception('not a postgis_geometry');
    }

    $this->geometries[] = $geometry;
  }

  /**
   * Get geometry at a given index.
   *
   * @param int $index
   * @return postgis_geometry
   */
  function get($index) {
    return $this->geometries[$index];
  }

  /**
   * Create a new set containing the diffrence of each geometry.
   *
   * @param postgis_geometry_set $set
   * @return postgis_geometry_set
   */
  function diff($set) {

    if (get_class($set) !== 'postgis_geometry_set') {
      throw new postgis_geometry_exception('not a postgis_geometry_set');
    }

    $diff_set = new postgis_geometry_set($this->type, $this->srid, $this->cardinality);

    foreach ($this->geometries as $index => $geometry) {
      if ($index < $set->count()) {
        $geometry = $geometry->diff($set->get($index));
      }
      $diff_set->add($geometry);
    }

    return $diff_set;
  }

  /**
   * Add geometries from text.
   *
   * @param String $wkt
   */
  function fromText($wkt) {

    if (empty($wkt)) {
      return;
    }

    try {
      // add GEOMETRYCOLLECTION if missing for OpenLayers compability
      if (stripos($wkt, 'GEOMETRYCOLLECTION') !== 0) {
        $wkt = 'GEOMETRYCOLLECTION('.$wkt.')';
      }

      if ($this->multi) {

        // convert geometrycollection with single geometries to multi geometry
        $wkt = db_query("SELECT ST_AsText(ST_Multi(ST_Collect(a.geom))) as wkt FROM (
          SELECT (ST_Dump(ST_GeomFromText(:wkt))).geom as geom) AS a",
          array(':wkt' => $wkt))->fetchField();

        $geometry = new postgis_geometry($this->type, $this->srid);
        $geometry->fromText($wkt);
        $this->geometries[] = $geometry;

      } else {

        // convert geometrycollection  to single geometries
        $rows = db_query("SELECT ST_AsText(a.geom) as wkt FROM (
          SELECT (ST_Dump(ST_GeomFromText(:wkt))).geom as geom) AS a;",
          array(':wkt' => $wkt));

        foreach ($rows as $row) {
          $geometry = new postgis_geometry($this->type, $this->srid);
          $geometry->fromText($row->wkt);
          $this->geometries[] = $geometry;
        }
      }
    }
    catch (PDOException $e) {

      // just add broken WKT, will be validated later.
      $geometry = new postgis_geometry($this->type, $this->srid);
      $geometry->fromText($wkt);
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

  function getText() {

    $wkt = array();
    foreach ($this->geometries as $geometry) {
      $wkt[] = $geometry->getText();
    }
    return implode(',', $wkt);
  }

  /**
   * Calculate bounding box of all geometries.
   *
   * @return <type>
   */
  function getBox() {

    $items = array();
    foreach ($this->geometries as $index => $geometry) {
      $items[':geo'.$index] = $geometry->getText();
    }
    $box = db_query("SELECT Box2D(ST_Union(".implode('::geometry , ', array_keys($items))."::geometry))",
        $items)->fetchField();

    return $box;
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

  function isEmpty() {
    return empty($this->geometries);
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
   * Calculate diffrence to a given geometry.
   *
   * @param postgis_geometry $geometry
   * @return postgis_geometry
   */
  function diff($geometry) {

    if (get_class($geometry) !== 'postgis_geometry') {
      throw new postgis_geometry_exception('not postgis_geometry');
    }

    $geoA = $this->getGeometry();
    $geoB = $geometry->getGeometry();

    $geoDiff = db_query("SELECT ST_Union(ST_Difference(:geoA, :geoB),ST_Difference(:geoB, :geoA))",
        array(':geoA' => $geoA, ':geoB' => $geoB))->fetchField();
    $geoType = db_query("SELECT GeometryType(:geo)", array(':geo' => $geoDiff))->fetchField();

    $diff = new postgis_geometry($geoType, $this->srid);
    $diff->fromGeometry($geoDiff);

    return $diff;
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
      return t('Unable to parse WKT: '.$geo);
    }
  }

  function length($unit) {

    $geo = is_null($this->wkt) ? $this->geometry : $this->wkt;

    $result = db_query("SELECT ST_Length(:geo, true)", array(':geo' => $geo))->fetchField();

    if ($unit === 'km') {
      $result = round($result/1000);
    }

    return $result;
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