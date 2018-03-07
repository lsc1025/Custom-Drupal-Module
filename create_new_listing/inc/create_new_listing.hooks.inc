<?php

/**
 * @file
 * Hook components of module create_new_listing
 */

 define('ANONYMOUS_UID', 0);
 define('LISTING_STATUS_DRAFT', 0);
 define('LISTING_STATUS_ACTIVE', 1);
 define('LISTING_STATUS_DEACTIVATED', 5);

 /**
  * Implements hook_menu()
  */
function create_new_listing_menu() {

	$items ['create_listing'] = array (
		'page callback' =>  'create_new_listing_create_listing_page',
		'title' =>	'Create New Listing', 
		'page arguments'  =>	array (1),
    'access callback' =>  'create_new_listing_access_create_listing_page',
    'access arguments' => array(1),
	);
  $items ['user/%user/subscription'] = array (
    'page callback' => 'create_new_listing_manage_subscription_page',
    'title' => 'Manage Subscription',
    'page arguments' => array(1),
		'access callback' => TRUE,
		'type' => MENU_LOCAL_TASK,
  );
  $items ['admin/config/content/create_new_listing'] = array(
    'page callback' => 'create_new_listing_admin_form',
    'title' => 'Listings',
    'description' => t('Configure how automated process with listing behaves.'),
    'page callback' => 'drupal_get_form',
    'page arguments' => array('create_new_listing_admin_form'),
    'access arguments' => array('administer listing setting'),
    'type' => MENU_NORMAL_ITEM,
  );

  return $items;

}

/**
 * Implements hook_permission().
 */
function create_new_listing_permission() {
  $permissions = array(
    'create classified listing' => array(
      'title' => t('Create new classified lisiting node'),
      'description' => t('User can create new classified lisiting node for any user.'),
      'restrict access' => TRUE,
    ),
    'administer listing setting' => array(
      'title' => t('Administer listings configuration'),
      'description' => t('Administer configuration related to listings'),
      'restrict access' => TRUE,
    ),
    'edit group license' => array(
      'title' => t('Edit business subscription'),
      'description' => t('Cancel or modify subscription for own group.'),
    )
  );

  return $permissions;
}

/**
 * Implements hook_commerce_order_update().
 *
 * Assign default value to field commerce_license_limit from the quota
 */
function create_new_listing_commerce_order_update($order) {

  if ($order->status == "pending" && isset($order->uid)) {

    $licenses = commerce_license_get_order_licenses($order);

    foreach ($licenses as $license) {

      // If license_limit is not set
      if ($license->commerce_license_limit == array()) {

        // Find quota limit from commerce_product
        foreach ($order->commerce_line_items['und'] as $line) {
          $line_item = commerce_line_item_load($line['line_item_id']);
          $product_id = $line_item->commerce_product['und'];
        }
        $quota_limit = (commerce_product_load($product_id[0]['product_id'])->cl_node_quota['und'][0]['value']);

        // Save license with the quota limit as default limit value
        $license->wrapper->commerce_license_limit->set($quota_limit);
        $license->wrapper->save();
      }
    }
  }
}

/**
 * Implements hook_form_alter()
 *
 * Hide status field in listing node edit form 
 */
function create_new_listing_form_alter(&$form, &$form_state, $form_id) {
  if ($form_id == 'listing_node_form') {
    $form['field_status']['#access'] = FALSE;
  }
  if (strpos($form_id, "commerce_cart_add_to_cart_form") === 0) {
    $form['#validate'][] = 'create_new_listing_add_to_cart_validate';
  }
}

/**
 * Implements hook_node_view()
 *
 * Add button for admin users or owner of the listing to cancel or mark listing as sold
 */
function create_new_listing_node_view($node, $view_mode, $langcode) {

  if ($node->type == 'listing') {
    global $user;

    // only check if user logged in
    if ($user->uid !== ANONYMOUS_UID) {
      // display actions for active listings
      if ($node->status == LISTING_STATUS_ACTIVE) {

        // if the user is the owner of current listing
        if ($node->uid == $user->uid) {
          $form = drupal_get_form('create_new_listing_edit_active_listing_form', $node->nid);
          $node->content['status_edit_widget'] = $form; 
        }

      // display actions for deactivated or draft listings
      } 
      elseif ($node->status == LISTING_STATUS_DEACTIVATED || $node->status == LISTING_STATUS_DRAFT) {

        if ($node->uid == $user->uid) {
          $form = drupal_get_form('create_new_listing_edit_draft_listing_form', $node->nid);
          $node->content['status_edit_widget'] = $form; 
        }
      
      }
    }
  }
}

/**
 * Implements hook_cron().
 */
function create_new_listing_cron() {

  $grace_time = variable_get('create_new_listing_grace_period', 2)*3600*24; // grace period in seconds

  $query = new EntityFieldQuery();
  $query
    ->entityCondition('entity_type', 'commerce_license')
    ->propertyCondition('status', COMMERCE_LICENSE_ACTIVE)
    ->propertyCondition('expires', commerce_license_get_time() - $grace_time, '<');
  $results = $query->execute();
  if (!empty($results['commerce_license'])) {
    foreach (array_keys($results['commerce_license']) as $license_id) {

      // All listings under the license will be unpublished and marked as expired
      $auto_expire_queue = DrupalQueue::get('create_new_listing_listing_expire');
      $task = array(
        'license_id' => $license_id,
        'title' => 'Expiring listings under license' . $license_id,
      );
      $auto_expire_queue->createItem($task);

      // mark license as expired 
      $license = entity_load_single('commerce_license', $license_id);
      $license->status = COMMERCE_LICENSE_EXPIRED;
      entity_save('commerce_license', $license);

      // @TODO do we need to check for pending billing cycle?

    }
  }
}

/**
 * Implements hook_advanced_queue_info().
 */
function create_new_listing_advanced_queue_info() {
  return array(
    'create_new_listing_listing_expire' => array(
      'worker callback' => 'create_new_listing_listing_expire_queue_process',
      'retry after' => 300,
      'max attempts' => 3,
      'start delay' => 0,
    ),
  );
}

/**
 * Implements hook_commerce_license_presave()
 */
function create_new_listing_commerce_license_presave($license) {
  
  // If new license found
  if ($license->status == COMMERCE_LICENSE_ACTIVE) {
    
    // check current active licenses
    $query = new EntityFieldQuery();
    $query
      ->entityCondition('entity_type', 'commerce_license')
      ->propertyCondition('type', 'role_node')
      ->propertyCondition('status', COMMERCE_LICENSE_ACTIVE)
      ->propertyCondition('uid', $license->uid);

    $result = $query->execute();

    if (!isset($result['commerce_license']))
      return;
    elseif (count($result['commerce_license']) > 1) {

      // find previous complete order
      $query = db_query('SELECT order_id FROM commerce_order WHERE uid = :uid ORDER BY order_id DESC', array(':uid' => $license->uid));

      $order_results = $query->fetchAll();
      
      if (count($order_results) < 3) {
        drupal_set_message(t('Invalid order data found!'), 'error');
        return;
      }

      array_shift($order_results); // pending order of new license
      $last_open_recurring_order = array_shift($order_results);
      $last_payment_order = array_shift($order_results);

      // find old license
      $old_license_id = array_shift($result['commerce_license'])->license_id;
      $old_license = entity_load_single('commerce_license', $old_license_id);

      $refundable = FALSE;

      // only issue refund for monthly membership and when user moving to annual subscription
      if ($old_license->expires - $old_license->granted <= 2678399) {
        $refundable = TRUE;
      }
      elseif ($license->expires - $license->granted >= 31535999) {
        $refundable = TRUE;
      }

      if ($refundable) {
        // calc and issue refund and credit
        $unused_time = $old_license->expires - commerce_license_get_time();

        if ($unused_time <= 0) {
          drupal_set_message(t('Invalid license found!'), 'error');
          return;
        }

        $last_transaction_id = get_transaction_id_by_order_id($last_payment_order->order_id);
        $last_transaction = commerce_payment_transaction_load($last_transaction_id);

        $total_amount = commerce_currency_amount_to_decimal($last_transaction->amount, $last_transaction->currency_code);
        $unused_amount = $total_amount * $unused_time / ($old_license->expires - $old_license->granted); // Total time is one month (30 days)

        create_new_listing_issue_stripe_refund($last_payment_order->order_id, $unused_amount);
      }

      // cancel open recurring order of old license
      $last_open_recurring_order = commerce_order_load($last_open_recurring_order->order_id);
      $last_open_recurring_order->status = 'canceled';
      commerce_order_save($last_open_recurring_order);

      // revoke old license
      $old_license->revoke();
      
    }
  }

}
