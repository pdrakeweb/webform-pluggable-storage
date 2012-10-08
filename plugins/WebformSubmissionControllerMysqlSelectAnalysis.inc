<?php

/**
 * @file Defines the WebformSubmissionControllerMysqlSelectAnalysis class.
 */

class WebformSubmissionControllerMysqlSelectAnalysis extends WebformSubmissionControllerMysql {

  public function getComponentSubmissionDataExtended($component, $sids = array(), $single = FALSE) {
    $options = _webform_select_options($component, TRUE);
    $show_other_results = $single;

    $option_operator = $show_other_results ? 'NOT IN' : 'IN';
    $query = db_select('webform_submitted_data', 'wsd', array('fetch' => PDO::FETCH_ASSOC))
      ->fields('wsd', array('data'))
      ->condition('nid', $component['nid'])
      ->condition('cid', $component['cid'])
      ->condition('data', '', '<>')
      ->condition('data', array_keys($options), $option_operator)
      ->groupBy('data');
    $query->addExpression('COUNT(data)', 'datacount');

    if (count($sids)) {
      $query->condition('sid', $sids, 'IN');
    }

    return $query->execute();
  }

  public function getComponentSubmissionDataCount($component, $sids = array()) {
    $count_query = db_select('webform_submitted_data', 'wsd', array('fetch' => PDO::FETCH_ASSOC))
      ->condition('nid', $component['nid'])
      ->condition('cid', $component['cid'])
      ->condition('data', '', '<>');
    $count_query->addExpression('COUNT(*)', 'datacount');
    if (count($sids)) {
      $count_query->condition('sid', $sids, 'IN');
    }

    return $count_query->execute();
  }

}
