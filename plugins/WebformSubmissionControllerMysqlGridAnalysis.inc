<?php

/**
 * @file Defines the WebformSubmissionControllerMysqlGridAnalysis class.
 */

class WebformSubmissionControllerMysqlGridAnalysis extends WebformSubmissionControllerMysql {

  public function getComponentSubmissionData($component, $sids = array(), $fields = array('no', 'data')) {
    // Generate a lookup table of results.
    $query = db_select('webform_submitted_data', 'wsd')
      ->fields('wsd', $fields)
      ->condition('nid', $component['nid'])
      ->condition('cid', $component['cid'])
      ->condition('data', '', '<>')
      ->groupBy('no')
      ->groupBy('data');
    $query->addExpression('COUNT(sid)', 'datacount');

    if (count($sids)) {
      $query->condition('sid', $sids, 'IN');
    }

    return $query->execute();
  }

}
