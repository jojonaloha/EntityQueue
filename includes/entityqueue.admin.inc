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
  $query->fields('ee', array('entity_type', 'entity_id', 'weight'))
    ->condition('ee.qid', $queue->qid)
    ->orderBy('ee.weight');
  $result = $query->execute();

  $entity_groups = array();
  $queue_items = array();
  $i = 0;
  foreach ($result as $row) {
    // Create groups of entities by entity type, so we can use entity_load
    $entity_groups[$row->entity_type][] = $row->entity_id;
    $queue_items[$row->entity_type . ':' . $row->entity_id] = $i;
    $i++;
  }

  // Load entities by type
  $entities_by_type = array();
  foreach ($entity_groups as $type => $ids) {
    $entities_by_type[$type] = entity_load($type, $ids);
  }
  unset($entity_groups);

  $entities = array();
  foreach ($queue_items as $key => $pos) {
    list($type, $id) = explode(':', $key);
    $entities[$pos] = $entities_by_type[$type][$id];
    // Set entity_type and entity_id so we know what they are later
    $entities[$pos]->entity_type = $type;
    $entities[$pos]->entity_id = $id;
  }
  unset($entities_by_type);
  unset($queue_items);

  return drupal_get_form('entityqueue_arrange_form_' . $queue->qid, $queue, $entities);
}

/**
 * Form builder.
 */
function entityqueue_arrange_form($form, &$form_state, $queue, $entities) {
  $form = array('#tree' => TRUE);

  $count = count($entities);
  $form['entities'] = array();
  $form['entities']['#theme'] = 'entityqueue_arrange_form_table';

  $form['entities']['#queue'] = (array) $queue;

  $form_state['queue'] = $queue;
  $form_state['entities'] = $entities;

  foreach ($entities as $weight => $entity) {
    $type = $entity->entity_type;
    $id = $entity->entity_id;
    $key = $type . ':' . $id;

    //$info = entity_get_info($type, $entity);
    $title = entity_label($type, $entity);
    $uri = entity_uri($type, $entity);
    $form['entities'][$key]['title'] = array(
      '#markup' => l($title, $uri),
    );

    $form['entities'][$key]['weight'] = array(
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
        'id' => 'entityqueue-remove-' . $type . '-' . $id,
      ),
      'query' => array(
        'destination' => drupal_get_destination(),
        'token' => drupal_get_token($key),
      ),
    );

    $form['entities'][$key]['remove'] = array(
      '#type' => 'link',
      '#title' => t('remove'),
      '#href' => 'entityqueue/' . $queue->qid . '/remove/' . $type . '/' . $id,
      '#options' => $options,
    );
  }

  // Field for adding entities to queue
  $form['add'] = array(
    '#type' => 'container',
    '#attributes' => array(
      'class' => array('container-inline'),
    ),
  );
  $form['add']['entity'] = array(
    '#type' => 'textfield',
    '#autocomplete_path' => 'entityqueue/autocomplete/' . $queue->qid,
    '#maxlength' => 1024,
    '#attributes' => array(
      'placeholder' => t('Enter the title of a entity to add it to the queue'),
      'class' => array('entityqueue-add-entity'),
    ),
  );
  $form['add']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Add entity'),
    '#submit' => array('entityqueue_arrange_form_add_submit'),
  );

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
