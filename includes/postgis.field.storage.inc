<?php

/**
 * @file
 * PostGIS implementation of the field storage API.
 */

/**
 * Implements hook_field_storage_info().
 */
function postgis_field_storage_info() {
  return array(
    'postgis_field_sql_storage' => array(
      'label' => t('PostGIS storage'),
      'description' => t('Stores fields in a PostGIS database, using the Field SQL storage module.'),
    ),
  );
}

function _postgis_field_storage_db_set_active($field) {

  global $databases;

  if (isset($databases['postgis']) && $field['type'] == 'postgis') {
    db_set_active('postgis');
  }
}

/**
 * Implements hook_field_storage_create_field().
 */
function postgis_field_storage_create_field($field) {
  _postgis_field_storage_db_set_active($field);
  // TODO: custom implementation of field_sql_storage_field_storage_create_field($field);
  db_set_active();
}

/**
 * Implements hook_field_update_forbid().
 *
 * Forbid any field update that changes column definitions if there is
 * any data.
 */
function postgis_field_update_forbid($field, $prior_field, $has_data) {
  _postgis_field_storage_db_set_active($field);
  // TODO: custom implementation of field_sql_storage_field_update_forbid($field, $prior_field, $has_data);
  db_set_active();
}

/**
 * Implements hook_field_storage_update_field().
 */
function postgis_field_storage_update_field($field, $prior_field, $has_data) {
  _postgis_field_storage_db_set_active($field);
  // TODO: custom implementation of field_sql_storage_field_storage_update_field($field, $prior_field, $has_data);
  db_set_active();
}

/**
 * Implements hook_field_storage_delete_field().
 */
function postgis_field_storage_delete_field($field) {
  _postgis_field_storage_db_set_active($field);
  // TODO: custom implementation of field_sql_storage_field_storage_delete_field($field);
  db_set_active();
}

/**
 * Implements hook_field_storage_load().
 */
function postgis_field_storage_load($entity_type, $entities, $age, $fields, $options) {
  _postgis_field_storage_db_set_active($field);
  // TODO: custom implementation of field_sql_storage_field_storage_load($entity_type, $entities, $age, $fields, $options);
  db_set_active();
}

/**
 * Implements hook_field_storage_write().
 */
function postgis_field_storage_write($entity_type, $entity, $op, $fields) {
  _postgis_field_storage_db_set_active($field);
  // TODO: custom implementation of field_sql_storage_field_storage_write($entity_type, $entity, $op, $fields);
  db_set_active();
}

/**
 * Implements hook_field_storage_delete().
 *
 * This function deletes data for all fields for an entity from the database.
 */
function postgis_field_storage_delete($entity_type, $entity, $fields) {
  _postgis_field_storage_db_set_active($field);
  // TODO: custom implementation of field_sql_storage_field_storage_delete($entity_type, $entity, $fields);
  db_set_active();
}

/**
 * Implements hook_field_storage_purge().
 *
 * This function deletes data from the database for a single field on
 * an entity.
 */
function postgis_field_storage_purge($entity_type, $entity, $field, $instance) {
  _postgis_field_storage_db_set_active($field);
  // TODO: custom implementation of field_sql_storage_field_storage_purge($entity_type, $entity, $field, $instance);
  db_set_active();
}

/**
 * Implements hook_field_storage_query().
 */
function postgis_field_storage_query(EntityFieldQuery $query) {
  _postgis_field_storage_db_set_active($field);
  $query = field_sql_storage_field_storage_query($query);
  db_set_active();
  return $query;
}

/**
 * Implements hook_field_storage_delete_revision().
 *
 * This function actually deletes the data from the database.
 */
function postgis_field_storage_delete_revision($entity_type, $entity, $fields) {
  _postgis_field_storage_db_set_active($field);
  // TODO: custom implementation of field_sql_storage_field_storage_delete_revision($entity_type, $entity, $fields);
  db_set_active();
}

/**
 * Implements hook_field_storage_delete_instance().
 *
 * This function simply marks for deletion all data associated with the field.
 */
function postgis_field_storage_delete_instance($instance) {
  _postgis_field_storage_db_set_active($field);
  // TODO: custom implementation of field_sql_storage_field_storage_delete_instance($instance);
  db_set_active();
}

/**
 * Implements hook_field_attach_rename_bundle().
 */
function postgis_field_attach_rename_bundle($entity_type, $bundle_old, $bundle_new) {
  _postgis_field_storage_db_set_active($field);
  // TODO: custom implementation of field_sql_storage_field_attach_rename_bundle($entity_type, $bundle_old, $bundle_new);
  db_set_active();
}

/**
 * Implements hook_field_storage_purge_field().
 *
 * All field data items and instances have already been purged, so all
 * that is left is to delete the table.
 */
function postgis_field_storage_purge_field($field) {
  _postgis_field_storage_db_set_active($field);
  // TODO: custom implementation of field_sql_storage_field_storage_purge_field($field);
  db_set_active();
}

/**
 * Implements hook_field_storage_details().
 */
function postgis_field_storage_details($field) {
  _postgis_field_storage_db_set_active($field);
  $details = field_sql_storage_field_storage_details($field); // TODO: custom implementation
  db_set_active();
  return $details;
}