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
    'count' => array('data' => t('Count')),
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
      'href' => 'admin/structure/entityqueue/' . $queue->name,
      'destination' => $destination,
    );

    if (entityqueue_access_admin_or_manage()) {
      $operations['edit'] = array(
        'title' => t('edit'),
        'href' => 'admin/structure/entityqueue/' . $queue->name . '/edit',
        'destination' => $destination,
      );

      $operations['delete'] = array(
        'title' => t('delete'),
        'href' => 'admin/structure/entityqueue/' . $queue->name . '/delete',
        'destination' => $destination,
      );
    }

    $rows[$queue->qid] = array(
      'title' => array(
        'data' => array(
          '#type' => 'link',
          '#title' => $queue->title,
          '#href' => 'admin/structure/entityqueue/' . $queue->name,
        ),
      ),
      'count' => check_plain($queue->count),
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

  $page['content']['queues'] = array(
    '#theme' => 'table',
    '#header' => $header,
    '#rows' => $rows,
    '#empty' => t('No queues available.'),
  );

  $page['content']['pager'] = array('#markup' => theme('pager'));

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

  $count = count($queue_items);
  $form['queue_items'] = array();
  $form['queue_items']['#theme'] = 'entityqueue_arrange_form_table';

  $form['queue_items']['#queue'] = (array) $queue;

  $form_state['queue'] = $queue;

  foreach ($queue_items as $weight => $queue_item) {
    $type = $queue_item->entity_type;
    $id = $queue_item->entity_id;
    $entity = $queue_item->entity;
    $key = $queue_item->qeid;

    $title = entity_label($queue_item->entity_type, $entity);
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
    '#validate' => array('entityqueue_arrange_form_add_validate'),
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
 *  Form validation handler.
 *
 *  @see  entityqueue_arrange_form()
 */
function entityqueue_arrange_form_add_validate($form, &$form_state) {
  $queue = $form_state['queue'];

  if (isset($form_state['values']['queue_item'])) {
    $value = $form_state['values']['queue_item'];
    preg_match('/\[(\w+):(\d+)\]/', $value, $matches);
    $entity_type = $matches[1];
    $entity_id = $matches[2];
    if (empty($entity_type) || empty($entity_id)) {
      form_set_error('', t('Please enter a valid @list', array(
        '@list' => implode(', ', array_keys($queue->bundles)),
      )));
    }
  }

  $queue_item = (object) array(
    'entity_type' => $entity_type,
    'entity_id' => $entity_id,
  );
  $form_state['values']['entityqueueitem'] = $queue_item;

  // @todo need to add field validation
  //entity_form_field_validate('entityqueueitem', $form, $form_state);
}

/**
 *  Form submission handler.
 *
 *  @see  entityqueue_arrange_form()
 */
function entityqueue_arrange_form_add_submit($form, &$form_state) {
  $queue = $form_state['queue'];

  // @todo build entityqueueitem
  entityqueue_add_item($queue, $form_state['values']['entityqueueitem']);
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

  // Roles
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

  // Entity types
  $all_entity_info = entity_get_info();
  $form['bundles'] = array(
    '#type' => 'fieldset',
    '#tree' => TRUE,
    '#title' => t('Entity Types'),
  );

  foreach ($all_entity_info as $entity_type => $info) {
    // Skip our entities to avoid an infinite loop
    if ($entity_type == 'entityqueue' || $entity_type == 'entityqueueitem') {
      continue;
    }

    $bundles = array();
    if (is_array($info['bundles']) && count($info['bundles'])) {
      foreach ($info['bundles'] as $value => $bundle_info) {
        $bundles[$value] = $bundle_info['label'];
      }
      $form['bundles'][$entity_type] = array(
        '#type' => 'checkboxes',
        '#title' => $info['label'],
        '#default_value' => isset($queue->bundles[$entity_type]) ? $queue->bundles[$entity_type] : array(),
        '#options' => $bundles,
        '#description' => t('Check each @entity type that can be added to the queue.', array('@entity' => $info['label'])),
      );
    }
  }

  $form['actions'] = array('#type' => 'actions');

  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Submit'),
    '#weight' => 5,
  );

  if (isset($queue->qid)) {
    $form['actions']['delete'] = array(
      '#type' => 'submit',
      '#value' => t('Delete'),
      '#validate' => array('nodequeue_edit_queue_form_delete_validate'),
      '#submit' => array('nodequeue_edit_queue_form_delete_submit'),
      '#weight' => 15,
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

  field_attach_form('entityqueue', $queue, $form, $form_state);
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
  foreach ($queue->bundles as $entity_type => $bundles) {
    $bundles = array_keys(array_filter($bundles));
    if (count($bundles)) {
      $queue->bundles[$entity_type] = $bundles;
    }
    else {
      unset($queue->bundles[$entity_type]);
    }
  }
  entity_form_field_validate('entityqueue', $form, $form_state);
}

/**
 * Submit function entityqueue_admin_edit()
 */
function entityqueue_admin_edit_submit($form, &$form_state) {
  $queue = (object) $form_state['values'];
  // Fix checkboxes.
  $queue->roles = array_keys(array_filter($queue->roles));
  foreach ($queue->bundles as $entity_type => $bundles) {
    $bundles = array_keys(array_filter($bundles));
    if (count($bundles)) {
      $queue->bundles[$entity_type] = $bundles;
    }
    else {
      unset($queue->bundles[$entity_type]);
    }
  }

  $status = entityqueue_save($queue);

  if ($status === ENTITYQUEUE_CREATED) {
    drupal_set_message(t('The queue @title has been created.', array(
      '@title' => $queue->title,
    )));
  }
  else {
    drupal_set_message(t('The queue @title has been updated.', array(
      '@title' => $queue->title,
    )));
  }
  $form_state['redirect'] = 'admin/structure/entityqueue';
}

/**
 * Page callback for autocomplete.
 */
function entityqueue_autocomplete() {
  $args = func_get_args();
  $queue = array_shift($args);
  $string = implode('/', $args);

  $entity_types = array_keys($queue->bundles);

  $matches = array();
  // Unfortunately there is no way to query across multiple entity base tables.
  // @see http://api.drupal.org/api/drupal/includes%21entity.inc/function/EntityFieldQuery%3A%3AentityCondition/7
  foreach ($entity_types as $entity_type) {
    $info = entity_get_info($entity_type);
    // If there is not a label property, we need to find the closest base
    // table property for querying
    $label_property = isset($info['entity keys']['label']) ? $info['entity keys']['label'] : FALSE;
    if (!$label_property) {
      $label_field = _entityqueue_find_substitute_label_field($info['base table'], array_values($info['entity_keys']));
    }
    else {
      $label_field = $label_property;
    }
    $bundles = $queue->bundles[$entity_type];

    $query = new EntityFieldQuery();
    $query->entityCondition('entity_type', $entity_type);
    // Taxonomy_term and comment entities don't support
    // entityCondition('bundle') queries.
    if (in_array($entity_type, array('taxonomy_term', 'comment'))) {
      // Add tags to the query so we can hook_query_alter() the SelectQuery,
      // since at this point we can't do innerJoin's and stuff.
      $query->addTag('entityqueue_entity_query');
      $query->addTag('entityqueue_' . $entity_type);
      $query->addMetaData('bundles', $bundles);
    }
    else {
      $query->entityCondition('bundle', $bundles, 'IN');
    }
    if ($label_field) {
      $query->propertyCondition($label_field, $string, 'CONTAINS');
    }

    $query->range(0, variable_get('entityqueue_autocomplete_limit', 10));
    $result = $query->execute();

    if (isset($result[$entity_type])) {
      // Get the id property.
      $id_property = $info['entity keys']['id'];
      // Check for a label callback.
      $label_callback = isset($info['label callback']) && function_exists($info['label callback']) ? $info['label callback'] : FALSE;

      // Get all the id's to reduce number of queries when we do an entity_load.
      $ids = array();
      foreach ($result[$entity_type] as $row) {
        $ids[] = $row->{$id_property};
      }
      $objects = entity_load($entity_type, $ids);
      foreach ($objects as $object) {
        // If the label is available, use that
        if ($label_property) {
          $label = $object->{$label_property};
        }
        // Otherwise use the label callback
        elseif ($label_callback) {
          $label = $label_callback($object);
        }
        // Add a "token" to our label so we can get our entity_type and id.
        $token = $entity_type . ':' . $object->{$id_property};
        $label .= ' [' . $token . ']';
        $matches[$token] = $label;
      }
    }
  }

  drupal_json_output(drupal_map_assoc($matches));
}

/**
 * Finds the best substitute field for using in search by "label"
 *
 * @param  string $base_table
 *   The name of the table to search, as provided to drupal_get_schema()
 * @param  array $ignore_fields
 *   Fields that cannot be used for the label.
 *
 * @return  string|false
 *   The field name, returns FALSE if no candidate field is found.
 */
function _entityqueue_find_substitute_label_field($base_table, $ignore_fields = NULL) {
  $schema = drupal_get_schema($base_table);
  // Most appropriate fields would be named one of the following.
  $best_candidates = array('title', 'label', 'name');

  if (!is_array($ignore_fields)) {
    $ignore_fields = array();
  }

  // Return the first field that matches one of our candidates
  $fields = array_diff(array_keys($schema['fields']), $ignore_fields);
  foreach ($best_candidates as $candidate) {
    if (in_array($candidate, $fields)) {
      return $candidate;
    }
  }

  // Otherwise, use the first varchar field
  foreach ($fields as $field) {
    if ($schema['fields'][$field]['type'] == 'varchar') {
      return $field;
    }
  }

  return FALSE;
}
