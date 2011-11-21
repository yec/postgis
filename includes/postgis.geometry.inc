<?php

/**
 * @file
 * PostGIS geometry classes.
 */

class PostgisGeometryException extends Exception {
}

/**
 * Provides a set of postgis_geometry with same geometry type and projection.
 */
class PostgisGeometrySet {

  private $geometries = array();
  private $cardinality, $type, $srid, $multi;

  /**
   * Creates an empty geometry set.
   *
   * @param string $type
   *   Geometry type.
   * @param int $srid
   *   Spatial projection.
   * @param int $cardinality
   *   Number of geometries.
   */
  function __construct($type, $srid, $cardinality = NULL) {
    $this->type = $type;
    $this->srid = $srid;
    $this->multi = stripos($type, 'multi') === 0;
    $this->cardinality = $cardinality;
  }

  /**
   * Validates each geometry.
   *
   * @param String $field_name
   *   Name of geometry form field.
   */
  function validate($field_name) {

    if ($this->count() > $this->cardinality) {
      form_set_error($field_name, t('To many geometries. Got @result, was expecting @cardinality.',
          array('@result' => $this->count(), '@cardinality' => $this->cardinality)));
    }

    foreach ($this->geometries as $geometry) {
      $result = $geometry->validate();
      if (!empty($result)) {
        form_set_error($field_name, check_plain($result));
      }
    }
  }

  /**
   * Returns length of all geometries.
   *
   * @param string $unit
   *   Abbreviation of the units, e.g. "km".
   *
   * @return int
   *   Length in given units.
   */
  function length($unit) {
    $length = 0;
    foreach ($this->geometries as $geometry) {
      $length += $geometry->length($unit);
    }
    return $length;
  }

  /**
   * Count geometries.
   */
  function count() {
    return count($this->geometries);
  }

  /**
   * Add a geometrie to geometries.
   */
  function add(PostgisGeometry $geometry) {
    $this->geometries[] = $geometry;
  }

  /**
   * Get geometry at a given index.
   */
  function get($index) {
    return $this->geometries[$index];
  }

  /**
   * Create a new set containing the diffrence of each geometry.
   */
  function diff($set) {

    if (get_class($set) !== 'postgis_geometry_set') {
      throw new PostgisGeometryException('not a postgis_geometry_set');
    }

    $diff_set = new PostgisGeometrySet($this->type, $this->srid, $this->cardinality);

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
   */
  function fromText($wkt) {

    if (empty($wkt)) {
      return;
    }

    try {
      // Add GEOMETRYCOLLECTION if missing for OpenLayers compability.
      if (stripos($wkt, 'GEOMETRYCOLLECTION') !== 0) {
        $wkt = 'GEOMETRYCOLLECTION(' . $wkt . ')';
      }

      if ($this->multi) {

        // Convert geometrycollection with single geometries to multi geometry.
        $wkt = db_query("SELECT ST_AsText(ST_Multi(ST_Collect(a.geom))) AS wkt 
          FROM (SELECT (ST_Dump(ST_GeomFromText(:wkt))).geom as geom) AS a",
          array(':wkt' => $wkt))->fetchField();

        $geometry = new PostgisGeometry($this->type, $this->srid);
        $geometry->fromText($wkt);
        $this->geometries[] = $geometry;

      }
      else {

        // Convert geometrycollection  to single geometries.
        $rows = db_query("SELECT ST_AsText(a.geom) as wkt FROM (
          SELECT (ST_Dump(ST_GeomFromText(:wkt))).geom as geom) AS a;",
          array(':wkt' => $wkt));

        foreach ($rows as $row) {
          $geometry = new PostgisGeometry($this->type, $this->srid);
          $geometry->fromText($row->wkt);
          $this->geometries[] = $geometry;
        }
      }
    }
    catch (PDOException $e) {

      // Just add broken WKT, will be validated later.
      $geometry = new PostgisGeometry($this->type, $this->srid);
      $geometry->fromText($wkt);
      $this->geometries[] = $geometry;
    }
  }

  /**
   * Add geometries from binary.
   *
   * @param array $items
   *   Geometries stored with key "geometry".
   */
  function fromGeometry($items) {

    foreach ($items as $item) {
      $geometry = new PostgisGeometry($this->type, $this->srid);
      $geometry->fromGeometry($item['geometry']);
      $this->geometries[] = $geometry;
    }
  }

  /**
   * Returns text representation of all geometries.
   */
  function getText() {

    $wkt = array();
    foreach ($this->geometries as $geometry) {
      $wkt[] = $geometry->getText();
    }
    return implode(',', $wkt);
  }

  /**
   * Calculate bounding box of all geometries.
   */
  function getBox($buffer = 0, $srid = NULL) {

    $items = array();
    foreach ($this->geometries as $index => $geometry) {
      $items[':geo' . $index] = $geometry->getText();
    }
    $keys = implode('::geometry , ', array_keys($items));

    $items[':buffer'] = $buffer;
    $items[':target_srid'] = empty($srid) ? $this->srid : $srid;
    $items[':source_srid'] = $this->srid;

    $box = db_query('SELECT Box2D(ST_Transform(ST_SetSRID(ST_Buffer(
      ST_Union(' . $keys . '::geometry), :buffer), :source_srid), :target_srid))',
      $items)->fetchField();

    return $box;
  }

  /**
   * Returns an array of geometries.
   *
   * Text representation is stored with key "wkt" and
   * binary representation is stored with key "geometry".
   *
   * @return array
   *   Array of text and binary representation of all geometries.
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

  /**
   * Checks if geometries are empty.
   */
  function isEmpty() {
    return empty($this->geometries);
  }
}

/**
 * Simple class for a PostGIS geometry.
 */
class PostgisGeometry {

  private $geometry, $wkt, $type, $srid;

  /**
   * Creates an empty geometry.
   *
   * @param String $type
   *   Geometry type.
   * @param int $srid
   *   Spatial projection.
   */
  function __construct($type, $srid) {
    $this->type = $type;
    $this->srid = $srid;
  }

  /**
   * Add geometry from text.
   */
  function fromText($wkt) {
    $this->wkt = $wkt;
    $this->geometry = NULL;
  }

  /**
   * Adds geometries from binary.
   */
  function fromGeometry($geometry) {
    $this->geometry = $geometry;
    $this->wkt = NULL;
  }

  /**
   * Returns text representation of geometry.
   */
  function getText() {
    if (is_null($this->wkt)) {
      $this->wkt = db_query("SELECT ST_AsText(:geometry)",
          array(':geometry' => $this->geometry))->fetchField();
    }
    return $this->wkt;
  }

  /**
   * Returns KML representation of geometry.
   */
  function getKML() {

    $geometry = is_null($this->wkt) ? $this->geometry : $this->wkt;

    return db_query("SELECT ST_AsKML(:geometry)", array(':geometry' => $geometry))->fetchField();
  }

  /**
   * Returns binary representation of geometry.
   */
  function getGeometry() {
    if (is_null($this->geometry)) {
      $this->geometry = db_query("SELECT ST_GeomFromText(:wkt, :srid)",
        array(':wkt' => $this->wkt, ':srid' => $this->srid))->fetchField();
    }
    return $this->geometry;
  }

  /**
   * Calculates diffrence to a given geometry.
   *
   * @param PostgisGeometry $geometry
   *   Geometry which this instance will be compared to.
   *
   * @return PostgisGeometry
   *   Geometry of diffrence.
   */
  function diff($geometry) {

    if (get_class($geometry) !== 'postgis_geometry') {
      throw new PostgisGeometryException('not postgis_geometry');
    }

    $geo_a = $this->getGeometry();
    $geo_b = $geometry->getGeometry();

    $geo_diff = db_query("SELECT ST_Union(ST_Difference(:geo_a, :geo_b), ST_Difference(:geo_b, :geo_a))",
        array(':geo_a' => $geo_a, ':geo_b' => $geo_b))->fetchField();
    $geo_type = db_query("SELECT GeometryType(:geo_diff)",
        array(':geo_diff' => $geo_diff))->fetchField();

    $diff = new PostgisGeometry($geo_type, $this->srid);
    $diff->fromGeometry($geo_diff);

    return $diff;
  }

  /**
   * Validates geometry.
   *
   * @return string|null
   *   Reason if not valid, otherwise null.
   */
  function validate() {

    $geo = is_null($this->wkt) ? $this->geometry : $this->wkt;

    try {
      $result = db_query("SELECT ST_GeometryType(:geo), ST_IsValid(:geo), ST_IsValidReason(:geo) as reason",
          array(':geo' => $geo))->fetchAssoc();

      // Return reason if geometry is not valid.
      if (!$result['st_isvalid']) {
        return t('Not a valid geometry: @reason.', array('@reason' => $result['reason']));
      }

      $result_type = drupal_strtolower(drupal_substr($result['st_geometrytype'], 3));

      // Return error if geometry type is unexpected.
      if ($result_type != $this->type) {
        return t('Wrong geometry type. Got @result, was expecting @type.',
          array('@result' => drupal_strtoupper($result_type), '@type' => drupal_strtoupper($this->type)));
      }
    }
    catch (PDOException $e) {
      // TODO: catch only WKT parse errors.
      return t('Unable to parse WKT: ' . $geo);
    }
  }

  /**
   * Returns length of geometry using ST_Length().
   *
   * @param string $unit
   *   Abbreviation of the units, e.g. "km".
   *
   * @return int
   *   Length in given units.
   */
  function length($unit) {

    $geo = is_null($this->wkt) ? $this->geometry : $this->wkt;

    $result = db_query("SELECT ST_Length(:geo, true)", array(':geo' => $geo))->fetchField();

    if ($unit === 'km') {
      $result = round($result / 1000);
    }

    return $result;
  }

  /**
   * Checks if geometry is empty.
   */
  function isEmpty() {
    $geo = is_null($this->wkt) ? $this->geometry : $this->wkt;
    return empty($geo);
  }
}

