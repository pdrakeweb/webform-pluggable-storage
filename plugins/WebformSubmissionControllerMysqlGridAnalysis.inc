<?php

/**
 * @file Defines the WebformSubmissionControllerMysqlGridAnalysis class.
 */

class WebformSubmissionControllerMysqlGridAnalysis extends WebformSubmissionControllerMysql {

  public function getComponentSubmissionData($component, $sids = array(), $fields = array('no', 'data')) {
    // Generate a lookup table of results.
    $placeholders = count($sids) ? array_fill(0, count($sids), "'%s'") : array();
    $sidfilter = count($sids) ? " AND sid in (" . implode(",", $placeholders) . ")" : "";
    $query = 'SELECT no, data, count(data) as datacount ' .
      ' FROM {webform_submitted_data} ' .
      ' WHERE nid = %d ' .
      ' AND cid = %d ' .
      " AND data != '' " . $sidfilter .
      ' GROUP BY no, data';
    $result = db_query($query, array_merge(array($component['nid'], $component['cid']), $sids));

    $return = array();
    while ($data = db_fetch_object($result)) {
      $return[] = $data;
    }
    return $return;
  }

}
