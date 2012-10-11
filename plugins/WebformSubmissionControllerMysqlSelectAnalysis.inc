<?php

/**
 * @file Defines the WebformSubmissionControllerMysqlSelectAnalysis class.
 */

class WebformSubmissionControllerMysqlSelectAnalysis extends WebformSubmissionControllerMysql {

  public function getComponentSubmissionDataExtended($component, $sids = array(), $single = FALSE) {
    $options = _webform_select_options($component, TRUE);
    $show_other_results = $single;

    $sid_placeholders = count($sids) ? array_fill(0, count($sids), "'%s'") : array();
    $sid_filter = count($sids) ? " AND sid IN (" . implode(",", $sid_placeholders) . ")" : "";

    $option_operator = $show_other_results ? 'NOT IN' : 'IN';
    $placeholders = count($options) ? array_fill(0, count($options), "'%s'") : array();
    $query = 'SELECT data, count(data) as datacount ' .
      ' FROM {webform_submitted_data} ' .
      ' WHERE nid = %d ' .
      ' AND cid = %d ' .
      " AND data != ''" . $sid_filter .
      ($placeholders ? ' AND data ' . $option_operator . ' (' . implode(',', $placeholders) . ')' : '') .
      ' GROUP BY data ';

    $result = db_query($query, array_merge(array($component['nid'], $component['cid']), $sids, array_keys($options)));

    $return = array();
    while ($data = db_fetch_array($result)) {
      $return[] = $data;
    }
    return $return;
  }

  public function getComponentSubmissionDataCount($component, $sids = array()) {
    $sid_placeholders = count($sids) ? array_fill(0, count($sids), "'%s'") : array();
    $sid_filter = count($sids) ? " AND sid IN (" . implode(",", $sid_placeholders) . ")" : "";

    $count_query = 'SELECT count(*) as datacount ' .
        ' FROM {webform_submitted_data} ' .
        ' WHERE nid = %d ' .
        ' AND cid = %d ' .
        " AND data != ''" . $sid_filter;

    return db_result(db_query($count_query, array_merge(array($component['nid'], $component['cid']), $sids)));
  }

}
