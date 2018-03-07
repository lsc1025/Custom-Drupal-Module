<?php

/**
 * @file
 * Rule actions related to listings
 */

/**
 * Implement hook_rules_action_info()
 *
 * Declare any meta data about actions for Rules
 */
function create_new_listing_rules_action_info() {
  $actions = array(
    'create_new_listing_actions_mark_listing_as_sold' => array(
      'label' => t('Mark indivisual listing as sold'),
      'group' => t('Listing'),
      'parameter' => array(
        'nid' => array(
          'type' => 'integer',
          'label' => t('Node id of listing to be marked as sold'),
        ),
      ),
    ),
    'create_new_listing_actions_cancel_listing' => array(
      'label' => t('Cancel and unpublish indivisual listing'),
      'group' => t('Listing'),
      'parameter' => array(
        'nid' => array(
          'type' => 'integer',
          'label' => t('Node id of listing to be cancelled'),
        ),
      ),
    ),
    'create_new_listing_actions_cancel_subscription' => array(
      'label' => t('Cancel single subscription'),
      'group' => t('Listing'),
      'parameter' => array(
        'nid' => array(
          'type' => 'integer',
          'label' => t('The user id of the subscription to be cancelled'),
        )
      )
    ),
    'create_new_listing_actions_publish_listing' => array(
      'label' => t('Publish indivisual listing'),
      'group' => t('Listing'),
      'parameter' => array(
        'nid' => array(
          'type' => 'integer',
          'label' => t('Node id of listing to be published'),
        )
      )
    ),
    'create_new_listing_actions_expire_listings_by_uid' => array(
      'label' => t('Expire listings by user'),
      'group' => t('Listing'),
      'parameter' => array(
        'license_id' => array(
          'type' => 'integer',
          'label' => t('Target user ID'),
        )
      )
    ),
    'create_new_listing_actions_renew_subscription' => array(
      'label' => t('Auto renew user\'s subscription'),
      'group' => t('Listing'),
      'parameter' => array(
        'uid' => array(
          'type' => 'integer',
          'label' => t('Target user ID'),
        )
      )
    ),
  );

  return $actions;
}

/**
 * Mark indivisual listing as sold
 *
 * @param $nid
 *  The node id of target listing node
 */
function create_new_listing_actions_mark_listing_as_sold($nid) {

  if (!is_numeric($nid)) {
    drupal_set_message(t('Invalid node id specified'), 'error');
    return;
  }

  $node = node_load($nid);
  // make sure the given nid belongs to a listing node
  if (!$node || $node->type != 'listing') {
    drupal_set_message(t('Incorrect node id specified'), 'error');
    return;
  }

  $node->status = 0;
  $node->field_status['und'][0]['value'] = 2;
  node_save($node);

  drupal_set_message(t('Marked listing #'.$nid.' as sold.'));

} 

/** 
 * Cancel and unpublish indivisual listing
 *
 * @param $nid
 *  The node id of target listing node
 */
function create_new_listing_actions_cancel_listing($nid) {

  if (!is_numeric($nid)) {
    drupal_set_message(t('Invalid node id specified'), 'error');
    return;
  }

  $node = node_load($nid);
  // make sure the given nid belongs to a listing node
  if (!$node || $node->type != 'listing') {
    drupal_set_message(t('Incorrect node id specified'), 'error');
    return;
  }

  $node->status = 0;
  $node->field_status['und'][0]['value'] = 5;
  node_save($node);

  drupal_set_message(t('Deactivated listing #'.$nid.'.'));
  
}

/**
 * Publish single listing
 *
 * @param $nid
 *  The node id of target listing node
 */
function create_new_listing_actions_publish_listing ($nid) {
  
  if (!is_numeric($nid)) {
    drupal_set_message(t('Invalid node id specified'), 'error');
    return;
  }

  $node = node_load($nid);
  // make sure the given nid belongs to a listing node
  if (!$node || $node->type != 'listing') {
    drupal_set_message(t('Incorrect node id specified'), 'error');
    return;
  }

  global $user;
  // check if the user is eligible for publish a listing
  if (user_access('create classified listing')
    || can_publish_listing_via_license($user->uid)) { 

    $node->status = 1;
    $node->field_status['und'][0]['value'] = 1;
    node_save($node);

    drupal_set_message(t('Published listing #'.$nid.'.'));

  } else {
    drupal_set_message(t('You have reached the listing limit of your license, consider upgrade.'), 'error');
  }

}

/**
 * Cancel the subscription for the user with specified uid
 *
 * @param $uid
 *  The target user id
 */
function create_new_listing_actions_cancel_subscription($uid) {
 
  if (!is_numeric($uid)) {
    drupal_set_message(t('Invalid user id specified'), 'error');
    return;
  }

  // Find all recurring open orders
  $query = new EntityFieldQuery();

  $query
    ->entityCondition('entity_type', 'commerce_order')
    ->propertyCondition('type', 'recurring')
    ->propertyCondition('status', 'recurring_open')
    ->propertyCondition('uid', $uid);

  $result = $query->execute()['commerce_order'];

  if (count($result) != 1) {
    drupal_set_message(t('Unable to find open billing cycle.'), 'error');
    return;
  }

  $order = commerce_order_load(array_shift($result)->order_id);
  $order->status = 'canceled';
  commerce_order_save($order);

  drupal_set_message(t('Your subscription has been cancelled.'));
}

/**
 * Expire all listings under a user and mark them as expired
 *
 * @param $uid
 *  The target user id
 */
function create_new_listing_actions_expire_listings_by_uid($uid) {

  if (!is_numeric($uid)) {
    drupal_set_message(t('Invalid uid specified.'), 'error');
    return;
  }

  $user = user_load($uid);

  if (!$user) {
    drupal_set_message(t('User not found!'), 'error');
  }

  $listings = db_query('SELECT nid FROM node WHERE type = :type AND uid = :uid AND status = :status', array(':type' => 'listing', ':uid' => $uid, ':status' => '1'))->fetchAll();

  foreach($listings as $nid) {

    $listing = node_load($nid->nid);
    $listing->status = 0;
    $listing->field_status['und'][0]['value'] = 3;
    node_save($listing);
    
  }

}

/**
 * Auto renew the subscription for the user with specified uid
 *
 * @param $uid
 *  The target user id
 */
function create_new_listing_actions_renew_subscription($uid) {
 
  if (!is_numeric($uid)) {
    drupal_set_message(t('Invalid user id specified'), 'error');
    return;
  }

  // Find all recurring open orders
  $query = db_query('SELECT order_id FROM commerce_order WHERE status = :status AND type = :type AND uid = :uid ORDER BY order_id DESC LIMIT 1', array(':status' => 'canceled', ':type' => 'recurring', ':uid' => $uid));

  $result = $query->fetchField();

  if (!$result) {
    drupal_set_message(t('Unable to find billing cycle.'), 'error');
    return;
  }

  $order = commerce_order_load($result);
  $order->status = 'recurring_open';
  commerce_order_save($order);

  drupal_set_message(t('Your subscription will be renewed automatically.'));
}
