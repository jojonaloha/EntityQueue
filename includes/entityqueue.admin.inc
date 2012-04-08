<?php
/**
 * @file
 * Page callbacks.
 */

/**
 * Lists existing queues.
 */
function entityqueue_view_queues() {
  $page = array(
    '#type' => 'page',
  );

  $admin_access = entityqueue_access_admin_or_manage() || user_access('populate all entityqueues');

  // Build sortable table header.
  $header = array(
    'title' => array('data' => t('Title'), 'field' => 'eq.title'),
    'size' => array('data' => t('Size'), 'field' => 'eq.size'),
    'operations' => array('data' => t('Operations')),
  );

  $query = db_select('entityqueue_queue', 'eq')
    ->extend('PagerDefault')
    ->extend('TableSort');
  // User can only view certain queues, add additional conditions
  if (!$admin_access) {
    global $user;

    // Restrict to queues with one of their roles
    $roles = array_keys((array) $user->roles);
    $query->innerJoin('entityqueue_roles', 'er', 'er.qid = eq.qid')
      ->condition('er.rid', $roles, 'IN')
      ->groupBy('eq.qid');
  }
  $qids = $query
    ->fields('eq', array('qid'))
    ->limit(50)
    ->orderByHeader($header)
    ->execute()
    ->fetchCol();
  $queues = entityqueue_load_multiple($qids);

  // Prepare the list of queues
  $destination = drupal_get_destination();
  $rows = array();
  foreach($queues as $queue) {
    $operations['view'] = array(
      'title' => t('view'),
      'href' => 'admin/structure/entityqueue/' . $queue->qid,
      'destination' => $destination,
    );

    if (entityqueue_access_admin_or_manage()) {
      $operations['edit'] = array(
        'title' => t('edit'),
        'href' => 'admin/structure/entityqueue/' . $queue->qid . '/edit',
        'destination' => $destination,
      );

      $operations['delete'] = array(
        'title' => t('delete'),
        'href' => 'admin/structure/entityqueue/' . $queue->qid . '/delete',
        'destination' => $destination,
      );
    }

    $rows[$queue->qid] = array(
      'title' => array(
        'data' => array(
          '#type' => 'link',
          '#title' => $queue->title,
          '#href' => 'admin/structure/entityqueue/' . $queue->qid,
        ),
      ),
      'size' => check_plain($queue->size),
      'operations' => array(
        'data' => array(
          '#theme' => 'links__entityqueue_operations',
          '#links' => $operations,
          '#attributes' => array('class' => array('links', 'inline')),
        ),
      ),
    );
  }

  $page['queues'] = array(
    '#theme' => 'table',
    '#header' => $header,
    '#rows' => $rows,
    '#empty' => t('No queues available.'),
  );

  $page['pager'] = array('#markup' => theme('pager'));
  dpm('testing');
  dpm($page);
  return $page;
}

/**
 * Form builder.
 */
function entityqueue_admin_settings($form, &$form_state) {
  $form = array();

  $form['entityqueue_autocomplete_limit'] = array(
    '#type' => 'textfield',
    '#title' => t('Entityqueue autocomplete limit'),
    '#size' => 2,
    '#default_value' => variable_get('entityqueue_autocomplete_limit', 10),
    '#description' => t('Number of entity titles to show in the autocomplete search results.'),
  );

  return system_settings_form($form);
}

/**
 * Page callback.
 */
function entityqueue_admin_view($queue) {
  drupal_set_title(t('Entity Queue @title', array('@title' => $queue->title)));

  $query = db_select('entityqueue_entity', 'ee')
    ->distinct();
  $query->fields('ee', array('qeid'))
    ->condition('ee.qid', $queue->qid)
    ->orderBy('ee.weight');
  $qeids = $query->execute()->fetchCol();

  // Load entityqueueitem entities
  $queue_items = entity_load('entityqueueitem', $qeids);

  return drupal_get_form('entityqueue_arrange_form_' . $queue->qid, $queue, $queue_items);
}

/**
 * Form builder.
 */
function entityqueue_arrange_form($form, &$form_state, $queue, $queue_items) {
  $form = array('#tree' => TRUE);

  $count = count($entities);
  $form['entities'] = array();
  $form['entities']['#theme'] = 'entityqueue_arrange_form_table';

  $form['entities']['#queue'] = (array) $queue;

  $form_state['queue'] = $queue;

  foreach ($queue_items as $weight => $queue_item) {
    $type = $queue_item->entity_type;
    $id = $queue_item->entity_id;
    $entity = $queue_item->entity;
    $key = $queue_item->qeid;

    $title = entity_label($entity->entity_type, $entity);
    $uri = entity_uri($type, $entity);
    $form['queue_items'][$key]['title'] = array(
      '#markup' => l($title, $uri),
    );

    $form['queue_items'][$key]['weight'] = array(
      '#type' => 'weight',
      '#delta' => $count,
      '#default_value' => $weight,
      '#attributes' => array(
        'class' => 'weight',
      ),
    );

    $options = array(
      '#attributes' => array(
        'title' => t('Remove from queue'),
        'style' => 'diaplay: none;',
        'class' => array('entityqueue-remove', 'entityqueue-remove-' . $type),
        'id' => 'entityqueue-remove-' . $key,
      ),
      'query' => array(
        'destination' => drupal_get_destination(),
        'token' => drupal_get_token($key),
      ),
    );

    $form['queue_items'][$key]['remove'] = array(
      '#type' => 'link',
      '#title' => t('remove'),
      '#href' => 'entityqueue/' . $queue->qid . '/remove/' . $type . '/' . $id,
      '#options' => $options,
    );

    $form_state['queue_items'][$key] = array(
      'entityqueueitem' => $queue_item,
    );
    field_attach_form('entityqueueitem', $queue_item, $form['queue_items'][$key], $form_state['queue_items'][$key]);
  }

  // Field for adding entities to queue
  $form['add'] = array(
    '#type' => 'container',
    '#attributes' => array(
      'class' => array('container-inline'),
    ),
  );
  $form['add']['queue_item'] = array(
    '#type' => 'textfield',
    '#autocomplete_path' => 'entityqueue/autocomplete/' . $queue->qid,
    '#maxlength' => 1024,
    '#attributes' => array(
      'placeholder' => t('Enter the title of an entity to add it to the queue'),
      'class' => array('entityqueue-add-entity'),
    ),
  );
  $form['add']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Add item'),
    '#submit' => array('entityqueue_arrange_form_add_submit'),
  );
  $form_state['add'] = array();
  $queue_item = new stdClass();
  field_attach_form('entityqueueitem', $queue_item, $form['add'], $form_state['add']);

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save'),
    '#submit' => array('entityqueue_arrange_form_submit'),
  );
  $form['actions']['reverse'] = array(
    '#type' => 'submit',
    '#value' => t('Reverse'),
    '#submit' => array('entityqueue_arrange_form_reverse_submit'),
  );
  $form['actions']['shuffle'] = array(
    '#type' => 'submit',
    '#value' => t('Shuffle'),
    '#submit' => array('entityqueue_arrange_form_shuffle_submit'),
  );
  $form['actions']['clear'] = array(
    '#type' => 'submit',
    '#value' => t('Clear'),
    '#submit' => array('entityqueue_arrange_form_clear_submit'),
  );

  if ($count == 0) {
    $form['actions']['submit']['#disabled'] = TRUE;
    $form['actions']['reverse']['#disabled'] = TRUE;
    $form['actions']['shuffle']['#disabled'] = TRUE;
    $form['actions']['clear']['#disabled'] = TRUE;
  }

  return $form;
}

/**
 * Add or edit a queue.
 */
function entityqueue_admin_edit($form, &$form_state, $queue = NULL) {
  // Adding queue
  if (!isset($queue)) {
    $queue = (object) array(
      'title' => '',
      'name' => NULL,
      'size' => 0,
      'roles' => array(),
    );
    drupal_set_title('Add Queue');
  }
  else {
    drupal_set_title(t('Edit Queue @title', array(
      '@title' => $queue->title,
    )));
  }

  $form['title'] = array(
    '#type' => 'textfield',
    '#title' => t('Title'),
    '#default_value' => $queue->title,
    '#size' => 50,
    '#required' => TRUE,
    '#maxlength' => 64,
    '#description' => t('Enter the name of the queue'),
  );

  $form['name'] = array(
    '#type' => 'machine_name',
    '#maxlength' => 32,
    '#machine_name' => array(
      'exists' => 'entityqueue_machine_name_exists',
      'source' => array('title'),
    ),
    '#description' => t('A unique machine-readable name for this queue. It must only contain lowercase letters, numbers, and underscores.'),
  );

  if (!empty($queue->name)) {
    $form['name']['#default_value'] = $queue->name;
    $form['name']['#disabled'] = TRUE;
    $form['name']['#value'] = $queue->name;
  }

  $form['size'] = array(
    '#type' => 'textfield',
    '#title' => t('Queue size'),
    '#default_value' => $queue->size,
    '#size' => 2,
    '#maxlength' => 2,
    '#description' => t('The maximum number of entities that can go in the queue. Enter 0 for no limit'),
  );

  $roles = user_roles();
  $manage_all_queues = user_roles(FALSE, 'populate all entityqueues');
  $manage_all_queues += user_roles(FALSE, 'manage entityqueues');
  $manage_all_queues += user_roles(FALSE, 'administer entityqueue');

  $form['roles'] = array(
    '#type' => 'checkboxes',
    '#title' => t('Roles'),
    '#default_value' => is_array($queue->roles) ? $queue->roles : array(),
    '#options' => $roles,
    '#description' => t('Check each role that can add entities to the queue.'),
  );

  foreach ($roles as $rid => $role) {
    $form['roles'][$rid] = array(
      '#type' => 'checkbox',
      '#title' => $role,
      '#default_value' => (isset($manage_all_queues[$rid]) || in_array($rid, $queue->roles)) ? TRUE : FALSE,
      '#disabled' => (isset($manage_all_queues[$rid])) ? TRUE : FALSE,
    );
  }

  // @todo add entity types/bundles

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Submit'),
  );

  if (isset($queue->qid)) {
    $form[] = array(
      '#type' => 'submit',
      '#value' => t('Delete'),
      '#validate' => array('nodequeue_edit_queue_form_delete_validate'),
      '#submit' => array('nodequeue_edit_queue_form_delete_submit'),
    );
    $form['qid'] = array(
      '#type' => 'value',
      '#value' => $queue->qid,
    );
    $form['count'] = array(
      '#type' => 'value',
      '#value' => $queue->count,
    );
  }

  return $form;
}

/**
 * Validate function for entityqueue_admin_edit()
 */
function entityqueue_admin_edit_validate($form, &$form_state) {
  if (empty($form_state['values']['title'])) {
    form_set_error('title', t('Please enter a title for this queue.'));
  }
  $queue = (object) $form_state['values'];
  // Fix checkboxes.
  $queue->roles = array_keys(array_filter($queue->roles));
  $queue->types = array_keys(array_filter($queue->types));
}

/**
 * Submit function entityqueue_admin_edit()
 */
function entityqueue_admin_edit_submit($form, &$form_state) {
  $queue = (object) $form_state['values'];
  // Fix checkboxes.
  $queue->roles = array_keys(array_filter($queue->roles));
  $queue->types = array_keys(array_filter($queue->types));

  $status = entityqueue_save($queue);

}
