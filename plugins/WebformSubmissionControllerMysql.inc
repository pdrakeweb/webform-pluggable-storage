<?php

/**
 * @file Defines the WebformSubmissionControllerMysql class.
 */

class WebformSubmissionControllerMysql implements WebformSubmissionController, WebformSubmissionAnalysisController {

  public $controller_type = 'db';

  public function getSubmissions($filters = array(), $header = NULL, $pager_from = 0, $pager_count = 0) {
    $submissions = array();

    if (!is_array($filters)) {
      $filters = array('nid' => $filters);
    }

    // UID filters need to be against a specific table.
    if (isset($filters['uid'])) {
      $filters['u.uid'] = $filters['uid'];
      unset($filters['uid']);
    }

    // No need to find SIDs if it was given to us.
    if (isset($filters['sid'])) {
      if(is_array($filters['sid'])){
        $sids = $filters['sid'];
      }
      else{
        $sids = array($filters['sid']);
      }
    }
    // Build the list of SIDs that need to be retrieved.
    else {
      $arguments = array_values($filters);
      $where = array();
      foreach ($filters as $column => $value) {
        $where[] = $column . ' = ' . (is_numeric($value) ? '%d' : "'%s'");
      }

      if (isset($filters['u.uid']) && $filters['u.uid'] === 0) {
        if (!empty($_SESSION['webform_submission'])) {
          $anonymous_sids = array_keys($_SESSION['webform_submission']);
          $where[] = 'sid IN (' . db_placeholders($anonymous_sids) . ')';
          $arguments = array_merge($arguments, $anonymous_sids);
        }
        else {
          $where[] = 'sid = 0';
        }
      }

      $where_clause = implode(' AND ', $where);
      $pager_query = 'SELECT sid FROM {webform_submissions} s LEFT JOIN {users} u ON u.uid = s.uid';
      $pager_query .= (!empty($where_clause) ? ' WHERE ' . $where_clause : '');

      if (is_array($header)) {
        $pager_query .= tablesort_sql($header);
      }
      else {
        $pager_query .= ' ORDER BY sid ASC';
      }

      if ($pager_count) {
        $result = db_query_range($pager_query, $arguments, $pager_from, $pager_count);
      }
      else {
        $result = db_query($pager_query, $arguments);
      }

      $sids = array();
      while ($row = db_fetch_object($result)) {
        $sids[] = $row->sid;
        $submissions[$row->sid] = FALSE;
      }
    }

    // If there are no submissions being retrieved, return an empty array.
    if (empty($sids)) {
      return $submissions;
    }

    // Query the required submission data.
    $query = 'SELECT s.*, sd.cid, sd.no, sd.data, u.name, u.mail, u.status ' .
        'FROM {webform_submitted_data} sd ' .
        'LEFT JOIN {webform_submissions} s ON s.sid = sd.sid ' .
        'LEFT JOIN {users} u ON u.uid = s.uid ' .
        'WHERE sd.sid IN (' . db_placeholders($sids) . ') ' .
        // By adding the NID to this query we allow MySQL to use the primary
        // key in webform_submitted_data for sorting (nid_sid_cid_no).
        (isset($filters['nid']) ? 'AND sd.nid = %d ' : '') .
        'ORDER BY sd.sid ASC, sd.cid ASC, sd.no ASC';

    $args = $sids;
    if (isset($filters['nid'])) {
      $args[] = $filters['nid'];
    }

    $result = db_query($query, $args);

    // Convert the queried rows into submissions.
    $previous = array();
    while ($row = db_fetch_object($result)) {
      if ($row->sid != $previous) {
        $submissions[$row->sid] = new stdClass();
        $submissions[$row->sid]->sid = $row->sid;
        $submissions[$row->sid]->nid = $row->nid;
        $submissions[$row->sid]->submitted = $row->submitted;
        $submissions[$row->sid]->remote_addr = $row->remote_addr;
        $submissions[$row->sid]->uid = $row->uid;
        $submissions[$row->sid]->name = $row->name;
        $submissions[$row->sid]->is_draft = $row->is_draft;
        $submissions[$row->sid]->data = array();
      }
      // CID may be NULL if this submission does not actually contain any data.
      if ($row->cid) {
        $submissions[$row->sid]->data[$row->cid]['value'][$row->no] = $row->data;
      }
      $previous = $row->sid;
    }

    foreach (module_implements('webform_submission_load') as $module) {
      $function = $module . '_webform_submission_load';
      $function($submissions);
    }

    return $submissions;
  }

  public function getSubmissionCount($nid, $uid = NULL, $reset = FALSE) {
    static $counts;

  if (!isset($counts[$nid][$uid]) || $reset) {
    $query = 'SELECT count(*) FROM {webform_submissions} WHERE nid = %d AND is_draft = 0';
    $arguments = array($nid);
    if ($uid !== NULL) {
      $query .= ' AND uid = %d';
      $arguments[] = $uid;
    }
    if ($uid === 0) {
      $submissions = isset($_SESSION['webform_submission']) ? $_SESSION['webform_submission'] : array();
      $query .= count($submissions) ? ' AND sid IN (' . db_placeholders($submissions) . ')' : ' AND sid = 0';
      $arguments = array_merge($arguments, array_keys($submissions));
    }

    $counts[$nid][$uid] = db_result(db_query($query, $arguments));
  }
  return $counts[$nid][$uid];
  }

  public function insertSubmission($node, $submission) {
    drupal_write_record('webform_submissions', $submission);
    $this->insertSubmissionData($node, $submission);
    return $submission;
  }

  public function updateSubmission($node, $submission) {
    // Update the main submission info.
    drupal_write_record('webform_submissions', $submission, 'sid');

      // If is draft, only delete data for components submitted, to
    // preserve any data from form pages not visited in this submission.
    if ($submission->is_draft) {
      $submitted_cids = array_keys($submission->data);
      if ($submitted_cids) {
        $placeholders = db_placeholders($submitted_cids);
        $arguments = array_merge(array($submission->sid), $submitted_cids);
        db_query("DELETE FROM {webform_submitted_data} WHERE sid = %d AND cid IN (" . $placeholders . ")", $arguments);
      }
    }
    else {
      db_query("DELETE FROM {webform_submitted_data} WHERE sid = %d", $submission->sid);
    }

    // Then re-add submission data to the database.
    $submission->is_new = FALSE;
    $this->insertSubmissionData($node, $submission);
    return $submission;
  }

  protected function insertSubmissionData($node, $submission) {
    foreach ($submission->data as $cid => $values) {
      foreach ($values['value'] as $delta => $value) {
        $data = array(
          'nid' => $node->webform['nid'],
          'sid' => $submission->sid,
          'cid' => $cid,
          'no' => $delta,
          'data' => is_null($value) ? '' : $value,
        );
        drupal_write_record('webform_submitted_data', $data);
      }
    }
  }

  public function deleteSubmissions($node, $submission) {
    db_query('DELETE FROM {webform_submitted_data} WHERE nid = %d AND sid = %d', $node->nid, $submission->sid);
    db_query('DELETE FROM {webform_submissions} WHERE nid = %d AND sid = %d', $node->nid, $submission->sid);
  }

  public function getPreviousSubmissionID($node, $submission) {
    return db_result(db_query('SELECT MAX(sid) FROM {webform_submissions} WHERE nid = %d AND sid < %d', array($node->nid, $submission->sid)));
  }

  public function getNextSubmissionID($node, $submission) {
    return db_result(db_query('SELECT MIN(sid) FROM {webform_submissions} WHERE nid = %d AND sid > %d', array($node->nid, $submission->sid)));
  }

  public function getComponentSubmissionData($component, $sids = array(), $fields = array('no', 'data')) {
    $placeholders = count($sids) ? array_fill(0, count($sids), "'%s'") : array();
    $sidfilter = count($sids) ? " AND sid in (" . implode(",", $placeholders) . ")" : "";
    $query = 'SELECT ' . implode(',', $fields) .
      ' FROM {webform_submitted_data} ' .
      ' WHERE nid = %d ' .
      ' AND  cid = %d ' . $sidfilter .
      ' ORDER BY sid ASC ';

    $result = db_query($query, array_merge(array($component['nid'], $component['cid']), $sids));

    $return = array();
    while ($data = db_fetch_array($result)) {
      $return[] = $data;
    }
    return $return;
  }

  public function deleteComponentSubmissionData($component) {
    db_query('DELETE FROM {webform_submitted_data} WHERE nid = %d AND cid = %d', $component['nid'], $component['cid']);
  }

  public function validateComponentSubmissionUnique($nid, $cid, $value, $sid = FALSE) {
    $count = db_result(db_query("SELECT count(*) FROM {webform_submitted_data} WHERE nid = %d AND cid = %d AND sid <> %d AND LOWER(data) = '%s'", $nid, $cid, $sid, $value));
    return (empty($count));
  }

  public function getDraftSubmissionID($nid, $uid) {
    $result = db_query("SELECT * FROM {webform_submissions} WHERE nid = %d AND uid = %d AND is_draft = 1 ORDER BY submitted DESC", $nid, $uid);
    $row = db_fetch_array($result);
    if (isset($row['sid'])) {
      return (int) $row['sid'];
    }
    return FALSE;
  }

  public function getComponentSubmissionMin($component) {
    return db_result(db_query('SELECT MIN(data) FROM {webform_submitted_data} WHERE nid = %d AND cid = %d', $component['nid'], $component['cid']));
  }

  public function getComponentSubmissionMax($component) {
    return db_result(db_query('SELECT MAX(data) FROM {webform_submitted_data} WHERE nid = %d AND cid = %d', $component['nid'], $component['cid']));
  }

  public function getComponentSubmissionAvg($component) {
    return db_result(db_query('SELECT AVG(data) FROM {webform_submitted_data} WHERE nid = %d AND cid = %d', $component['nid'], $component['cid']));
  }

  public function getComponentSubmissionCount($component, $count_type = 'all') {
    switch($count_type) {
      case 'empty':
        $count_type_sql = ' AND wsd.data = ""';
        break;

      case 'notempty':
        $count_type_sql = ' AND wsd.data != ""';
        break;

      default:
      case 'all':
        $count_type_sql = '';
        break;
    }
    return db_result(db_query('SELECT COUNT(*) FROM {webform_submitted_data} wsd WHERE wsd.nid = %d AND wsd.cid = %d' . $count_type_sql, $component['nid'], $component['cid']));
  }

}
