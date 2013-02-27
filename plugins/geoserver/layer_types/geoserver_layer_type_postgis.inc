<?php
/**
 * @file
 * base GeoServer layer type for PostGIS layers.
 */

abstract class geoserver_layer_type_postgis extends geoserver_layer_type_feature_type {

  public $datastore = NULL;

  /**
   * Constructor.
   */
  function __construct($layer = array()) {
    parent::__construct($layer);
    $this->datastore = variable_get('geoserver_postgis_datastore', '');
  }


  /**
   * Export to remote feature type definition.
   */
  function to_feature_type() {
    $feature_type = parent::to_feature_type();
    $feature_type['featureType']['store'] = array(
        '@class' => 'dataStore',
        'name' => $this->datastore
    );
    return $feature_type;
  }

  /**
   * get the max bbox for a srid
   * tries to calculate maximum bounding box for defined projection.
   */
  protected function max_boundingbox_for_srid($srid) {
    $bbox = array('', '', '', '');

    if ($srid == '4326') {
      $bbox = array('-180', '-90', '180', '90');
    }
    elseif ($srid == '900913') {
      $bbox = array('-20037508', '-20037508', '20037508', '20037508');
    }
    else {
      // Try to calculate maximum bounding box for defined projection.
      try {
        $extent = db_query("select st_extent(st_transform(st_setsrid(st_geomfromtext('polygon((-180 -90,-180 90, 180 90, -180 -90))'), 4326), :srid::int))",
          array(':srid' => $srid))->fetchField();
        $bbox = preg_split('/[\, ]/', drupal_substr($extent, 4, -1));
      } catch (PDOException $exc) {
        // TODO
      }
    }
    return $bbox;
  }

  /**
   * Layer type settings form.
   *
   * TODO: changing geoserver_postgis_datastore here affects all sublayers
   */
  function settings_form() {

    $datastores = array('');
    $workspace  = variable_get('geoserver_workspace', '');

    $result = geoserver_get('rest/workspaces/' . $workspace . '/datastores.json');
    if ($result->code == 200 && isset($result->data->dataStores)) {
      foreach ($result->data->dataStores->dataStore as $datastore) {
        $datastores[$datastore->name] = $datastore->name;
      }
    }

    return array(
      'geoserver_postgis_datastore' => array(
        '#type' => 'select',
        '#title' => t('Datastore'),
        '#options' => $datastores,
        '#description' => t('Select the PostGIS datastore where Drupal stores its data. This one needs to be created manually for now.'),
        '#default_value' => variable_get('geoserver_postgis_datastore', ''),
        '#suffix' => '</div>',
      ),
    );
  }
}


// vim: tabstop=2 shiftwidth=2
