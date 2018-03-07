<?php

/**
 * @file
 * Form components of module create_new_listing
 */

 /**
 * Configuration form of create_new_listing
 */
function create_new_listing_admin_form($form, &$form_state) {

  $form = array(
    'create_new_listing_grace_period' => array(
      '#type' => 'textfield',
      '#title' => t('Grace Period'),
      '#default_value' => variable_get('create_new_listing_grace_period', 2), // default value is 2 days
      '#description' => t('The grace period before expiring listings in days.'),
      '#maxlength' => 2,
    ),
  );

  return system_settings_form($form);

}
 
/**
 * Returns the form that creates the classified listing node
 */
function create_new_listing_create_listing_form($form, &$form_state, $account, $license, $usage) {

  $form['#user'] = $account;
  $form['#limit'] = 0;

  if ($license == 'admin access') {
    $form['#limit'] = -1; // skip limit checking for user with privilege
    $form['info'] = array(
      '#type' => 'item',
      '#markup' => t('You are creating listing for user: <b>'.$account->name.'</b>'),
    );
  } else {
    $limit = $license->commerce_license_limit['und'][0]['value'];
    $form['#limit'] = $limit;
    if ($limit < 0) {
      $limit = t('Unlimited');
    }
    $form['info'] = array(
      '#type' => 'item',
      '#markup' => t('User name: <b>'.$account->name.'</b><br/>Current usage: <b>'.$usage.' Listing(s)</b><br/>Listing Limit: <b>'.$limit.'</b>'),
    );
  }

  $form['#parents'] = array(); // a required element to use 'field_default_form' function below.
  $form['title'] = array(
    '#type' => 'textfield',
    '#title' => t('Listing Title'),
    '#required' => TRUE,
  );

  foreach (field_info_instances('node', 'listing') as $field_name => $value) {

    if ($field_name == 'field_status') // hide status field
      continue;

    $field = field_info_field($field_name);
    $instance = field_info_instance('node', $field_name, 'listing');
    $items = array();

    $form[$field_name] = field_default_form('node', NULL, $field, $instance, LANGUAGE_NONE, $items, $form, $form_state);
  }

  $form += array (
    'publishoption' => array(
      '#type' => 'select',
      '#options' => array(
        'Save Draft',
        'Publish',
      ),
      '#title' => 'Publish Option',
    ),
    'submit' => array(
      '#type' => 'submit',
      '#value' => t('Save')
    ),
    'cancel' => array(
		  '#type' => 'button',
		  '#value' => t('Cancel'),
		  '#attributes' => array(
			  'onClick' => 'history.go(-1); event.preventDefault();'
		  ),
    ),
  );

  return $form;

}

/**
 * Validation action for form create_new_listing_create_listing_form
 */
function create_new_listing_create_listing_form_validate($form, &$form_state) {
 
  return TRUE;

}

/**
 * Submission action for form create_new_listing_create_listing_form
 */
function create_new_listing_create_listing_form_submit($form, &$form_state) {

  // run query again to ensure user have enough credits
  if (get_usage_by_uid($form['#user']->uid) >= $form['#limit'] && $form['#limit'] >= 0) {
    drupal_set_message(t('You have reached the listing limit of your license, consider upgrade.'), 'error');
    return;
  }

  // Create a new listing object
  $values = $form_state['values'];
  $new_node = new stdClass();
  $new_node->type = 'listing';
  node_object_prepare($new_node);
  $new_node->uid = $form['#user']->uid;
  $new_node->language = LANGUAGE_NONE;

  $new_node->title = $values['title']; // Set the listing title
  $new_node->status = $values['publishoption']; // Set the listing status accordingly
  $new_node->promote = $values['publishoption']; // Make published listing promoted to front page
  $new_node->field_status['und'][0]['value'] = $values['publishoption'];

  $node = node_submit($new_node);
  node_save($node); // save node to get nid

  // attach other fields to the new node
  foreach(field_info_instances('node', 'listing') as $field_name => $value) {
    if ($field_name == 'field_status')
      continue; // skip field_status
    if (!isset($form_state['values'][$field_name])) {   
      $node->$field_name = $form_state['values'][$field_name];     
    }
  }

  field_attach_update('node', $node);
  entity_get_controller('node')->resetCache(array($node->nid)); // clear cache for change to take effect
  drupal_set_message(t('Listing '.$values['title'].' created'));

  // redirect to lsiting page
  $form_state['redirect'] = array('node/'.$node->nid);

}

/**
 * Widget for listing owners to change the listing status
 */ 
function create_new_listing_edit_active_listing_form($form, &$form_state, $nid) {

  $form['#nid'] = $nid;

  $form += array(
    'mark_as_sold' => array(
      '#type' => 'submit',
      '#value' => t('Mark as Sold'),
      '#submit' => array('mark_as_sold_submit'),
    ),
    'cancel_listing' => array(
      '#type' => 'submit',
      '#value' => t('Cancel Listing'),
      '#submit' => array('cancel_listing_submit'),
    ),
  );

  return $form;

}

/**
 * Submission action of create_new_listing_edit_own_listing_form
 */
function mark_as_sold_submit($form, &$form_state) {

  create_new_listing_actions_mark_listing_as_sold($form['#nid']);
  
}

/**
 * Submission action of create_new_listing_edit_own_listing_form
 */
function cancel_listing_submit($form, &$form_state) {

  create_new_listing_actions_cancel_listing($form['#nid']);

}

/**
 * Widget for deactivated listings
 */
function create_new_listing_edit_draft_listing_form($form, &$form_state, $nid) {

  $form['#nid'] = $nid;

  $form += array(
    'publish' => array(
      '#type' => 'submit',
      '#value' => t('Publish Listing'),
    ),
  );

  return $form;

}

/**
 * Submission actions of create_new_listing_edit_draft_listing_form
 */
function create_new_listing_edit_draft_listing_form_submit($form, &$form_state) {

  create_new_listing_actions_publish_listing($form['#nid']);

}

/**
 * Form for user to manage own subscription
 */
function create_new_listing_manage_subscription_form($form, &$form_state, $license) {

  if (!$license) {
    $form['info'] = array(
      '#type' => 'item',
      '#markup' => t('No active subscription is associated with your business.'),
    );
    return $form;
  }

  $form['#uid'] = $license->uid;
  $product = commerce_product_load($license->product_id);

  $schd_payment = get_next_payment_by_uid($license->uid);

  $form += array(
    'info' => array(
      '#type' => 'item',
      '#markup' => t('<b>Subscription Details:</b><br />Subscription Type: '.$product->title.'<br />Listing Limit: '.$license->commerce_license_limit['und'][0]['value'].'<br />Date of Expiry: '.format_date($license->expires, 'short').'<br />Current Listing Usage: '.get_usage_by_uid($license->uid)),
    ),
  );

  if (user_access('edit group license')) {

    $form += array (
      'change' => array(
        '#type' => 'submit',
        '#value' => t('Change Subscription'),
        '#submit' => array('change_subscription_submit'),
      ),
    );

    if ($schd_payment == 'N/A') {
      $form['info']['#markup'] .= t('<br />Your subscription will not be renewed automatically.');
    } else {
      $form['info']['#markup'] .= t('<br />Next Scheduled Payment: '.$schd_payment['amount'].'<br />Scheduled Date: '.$schd_payment['schedule']);
    }

    if (isset($schd_payment['schedule'])) {
      $form += array (
        'cancel' => array(
          '#type' => 'submit',
          '#value' => t('Cancel Subscription'),
          '#attributes' => array('onclick' => 'if(!confirm("'.t("All of your active listings will be unpublished after your current license expires, continue?").'")){return false;}'),
          '#submit' => array('cancel_subscription_submit'),
        ),
      );
    } else {
      $form += array (
        'renew' => array(
          '#type' => 'submit',
          '#value' => t('Renew Subscription Automatically'),
          '#submit' => array('renew_subscription_submit'),
        )
      );
    }

  }
  return $form;

}

/**
 * Submission action of create_new_listing_manage_subscription_form
 */
function change_subscription_submit($form, &$form_state) {

  $form_state['redirect'] = array('');

}

/**
 * Submission action of create_new_listing_manage_subscription_form
 */
function cancel_subscription_submit($form, &$form_state) {

  create_new_listing_actions_cancel_subscription($form['#uid']);

}

/**
 * Submission action of create_new_listing_manage_subscription_form
 */
function renew_subscription_submit($form, &$form_state) {

  create_new_listing_actions_renew_subscription($form['#uid']);

}
