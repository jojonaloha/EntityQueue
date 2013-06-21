<?php
/**
 * @file
 * Theme functions for entityqueue module.
 */

function theme_entityqueue_arrange_form_table($variables) {
  $form = $variables['form'];

  $output = '';

  // @todo add custom css?

  $table_id = 'entityqueue-dragdrop-' . $form['#queue']['qid'];
  $table_classes = array(
    'entityqueue-dragdrop',
    'entityqueue-dragdrop-name-' . $form['#queue']['name'],
  );
  drupal_add_tabledrag($table_id, 'order', 'sibling', 'weight');

  $rows = array();
  $counter = 1;
  foreach (element_children($form) as $key) {
    if (isset($form[$key]['title'])) {
      $operations = drupal_render($form[$key]['remove']);
      $rows[] = array(
        'data' => array(
          drupal_render($form[$key]['title']),
          drupal_render($form[$key]['weight']),
          drupal_render($form[$key]),
          $operations,
        ),
        'class' => array('draggable'),
      );
    }
    $counter++;
  }
  if (empty($rows)) {
    $rows[] = array(array('data' => t('No entities in this queue.'), 'colspan' => 4));
  }
  $header = array(
    t('Title'),
    t('Weight'),
    t('Fields'),
    t('Operations'),
  );
  $output .= theme('table', array('header' => $header, 'rows' => $rows, 'attributes' => array('id' => $table_id, 'class' => $table_classes)));

  return $output;
}
