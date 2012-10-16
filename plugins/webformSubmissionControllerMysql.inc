<?php

/**
 * @file Defines the WebformSubmissionControllerMysql class.
 */

class WebformSubmissionControllerMysql implements WebformSubmissionController, WebformSubmissionAnalysisController {

  public $controller_type = 'db';

  public function getSubmissions($filters = array(), $header = NULL, $pager_count = 0) {
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
      $sids = array($filters['sid']);
    }
    // Build the list of SIDs that need to be retrieved.
    else {
      $pager_query = db_select('webform_submissions', 'ws')->fields('ws', array('sid'));
      foreach ($filters as $column => $value) {
        $pager_query->condition($column, $value);
      }

      if (isset($filters['u.uid']) || !empty($header)) {
        // Join to the users table for sorting by user name.
        $pager_query->leftJoin('users', 'u', 'u.uid = ws.uid');
      }

      if (isset($filters['u.uid']) && $filters['u.uid'] === 0) {
        if (!empty($_SESSION['webform_submission'])) {
          $anonymous_sids = array_keys($_SESSION['webform_submission']);
          $pager_query->condition('sid', $anonymous_sids, 'IN');
        }
        else {
          $pager_query->condition('sid', 0);
        }
      }

      if (is_array($header)) {
        // Extending the query instatiates a new query object.
        $pager_query = $pager_query->extend('TableSort');
        $pager_query->orderByHeader($header);
      }
      else {
        $pager_query->orderBy('sid', 'ASC');
      }

      if ($pager_count) {
        // Extending the query instatiates a new query object.
        $pager_query = $pager_query->extend('PagerDefault');
        $pager_query->limit($pager_count);
      }
      $result = $pager_query->execute();

      $sids = array();
      foreach ($result as $row) {
        $sids[] = $row->sid;
        $submissions[$row->sid] = FALSE;
      }
    }

    // If there are no submissions being retrieved, return an empty array.
    if (empty($sids)) {
      return $submissions;
    }

    // Query the required submission data.
    $query = db_select('webform_submitted_data', 'sd');
    $query->leftJoin('webform_submissions', 's', 's.sid = sd.sid');
    $query->leftJoin('users', 'u', 'u.uid = s.uid');
    $query
      ->fields('s')
      ->fields('sd', array('cid', 'no', 'data'))
      ->fields('u', array('name'))
      ->condition('sd.sid', $sids, 'IN')
      ->orderBy('sd.sid', 'ASC')
      ->orderBy('sd.cid', 'ASC')
      ->orderBy('sd.no', 'ASC');

    // By adding the NID to this query we allow MySQL to use the primary key on
    // in webform_submitted_data for sorting (nid_sid_cid_no).
    if (isset($filters['nid'])) {
      $query->condition('sd.nid', $filters['nid']);
    }

    $result = $query->execute();

    // Convert the queried rows into submissions.
    $previous = 0;
    foreach ($result as $row) {
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
        $submissions[$row->sid]->data[$row->cid][$row->no] = $row->data;
      }
      $previous = $row->sid;
    }

    foreach (module_implements('webform_submission_load') as $module) {
      $function = $module . '_webform_submission_load';
      $function($submissions);
    }

    return $submissions;
  }

  public function getSubmissionCount($nid, $uid = NULL, $submission_interval = -1, $reset = FALSE) {
    static $counts;

    if (!isset($counts[$nid][$uid]) || $reset) {
      $query = db_select('webform_submissions', 'ws')
        ->addTag('webform_get_submission_count')
        ->condition('ws.nid', $nid)
        ->condition('ws.is_draft', 0);
      $arguments = array($nid);
      if (!empty($uid)) {
        $query->condition('ws.uid', $uid);
      }
      else {
        $submissions = isset($_SESSION['webform_submission']) ? $_SESSION['webform_submission'] : NULL;
        if ($submissions) {
          $query->condition('ws.sid', $submissions, 'IN');
        }
        else {
          // Intentionally never match anything if the anonymous user has no
          // submissions.
          $query->condition('ws.sid', 0);
        }
      }
      if ($submission_interval != -1) {
        $query->condition('submitted', REQUEST_TIME - $submission_interval, '>');
      }

      $counts[$nid][$uid] = $query->countQuery()->execute()->fetchField();
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
        db_delete('webform_submitted_data')
          ->condition('sid', $submission->sid)
          ->condition('cid', $submitted_cids, 'IN')
          ->execute();
      }
    }
    else {
      db_delete('webform_submitted_data')
        ->condition('sid', $submission->sid)
        ->execute();
    }

    // Then re-add submission data to the database.
    $this->insertSubmissionData($node, $submission);
    return $submission;
  }

  protected function insertSubmissionData($node, $submission) {
    foreach ($submission->data as $cid => $values) {
      foreach ($values as $delta => $value) {
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

  public function deleteSubmissions($node, $submission = FALSE) {
    $data_delete = db_delete('webform_submitted_data')
      ->condition('nid', $node->nid);
    if ($submission != FALSE) {
      $data_delete->condition('sid', $submission->sid);
    }
    $data_delete->execute();

    $submission_delete = db_delete('webform_submissions')
      ->condition('nid', $node->nid);
    if ($submission != FALSE) {
      $submission_delete->condition('sid', $submission->sid);
    }
    $submission_delete->execute();
  }

  public function getPreviousSubmissionID($node, $submission) {
    $previous_query = db_select('webform_submissions')
      ->condition('nid', $node->nid)
      ->condition('sid', $submission->sid, '<');
    $previous_query->addExpression('MAX(sid)');
    return $previous_query->execute()->fetchField();
  }

  public function getNextSubmissionID($node, $submission) {
    $next_query = db_select('webform_submissions')
      ->condition('nid', $node->nid)
      ->condition('sid', $submission->sid, '<');
    $next_query->addExpression('MIN(sid)');
    return $next_query->execute()->fetchField();
  }

  public function getComponentSubmissionData($component, $sids = array(), $fields = array('no', 'data')) {
    $query = db_select('webform_submitted_data', 'wsd', array('fetch' => PDO::FETCH_ASSOC))
      ->fields('wsd', $fields)
      ->condition('nid', $component['nid'])
      ->condition('cid', $component['cid']);

    if (count($sids)) {
      $query->condition('sid', $sids, 'IN');
    }

    return $query->execute();
  }

  public function deleteComponentSubmissionData($component) {
    db_delete('webform_submitted_data')
      ->condition('nid', $component['nid'])
      ->condition('cid', $component['cid'])
      ->execute();
  }

  public function validateComponentSubmissionUnique($nid, $cid, $value, $sid = FALSE) {
    $query = db_select('webform_submitted_data')
      ->fields('webform_submitted_data', array('sid'))
      ->condition('nid', $nid)
      ->condition('cid', $cid)
      ->condition('data', $value)
      ->range(0, 1); // More efficient than using countQuery() for data checks.
    if ($sid) {
      $query->condition('sid', $sid, '<>');
    }
    return ($query->execute()->fetchField() == 0);
  }

  public function getDraftSubmissionID($nid, $uid) {
    return db_select('webform_submissions')
      ->fields('webform_submissions', array('sid'))
      ->condition('nid', $nid)
      ->condition('uid', $uid)
      ->condition('is_draft', 1)
      ->orderBy('submitted', 'DESC')
      ->execute()
      ->fetchField();
  }

  public function getComponentSubmissionMin($component, $sids = array()) {
    $query = db_select('webform_submitted_data', 'wsd')
      ->addExpression('MIN(*)', 'value')
      ->condition('nid', $component['nid'])
      ->condition('cid', $component['cid']);

    if (count($sids)) {
      $query->condition('sid', $sids, 'IN');
    }

    return $query->execute()->fetchField();
  }

  public function getComponentSubmissionMax($component) {
    $query = db_select('webform_submitted_data', 'wsd')
      ->addExpression('MAX(*)', 'value')
      ->condition('nid', $component['nid'])
      ->condition('cid', $component['cid']);

    if (count($sids)) {
      $query->condition('sid', $sids, 'IN');
    }

    return $query->execute()->fetchField();
  }

  public function getComponentSubmissionAvg($component) {
    $query = db_select('webform_submitted_data', 'wsd')
      ->addExpression('AVG(*)', 'value')
      ->condition('nid', $component['nid'])
      ->condition('cid', $component['cid']);

    if (count($sids)) {
      $query->condition('sid', $sids, 'IN');
    }

    return $query->execute()->fetchField();
  }

  public function getComponentSubmissionCount($component, $count_type = 'all') {
    $query = db_select('webform_submitted_data', 'wsd')
      ->addExpression('COUNT(*)', 'value')
      ->condition('nid', $component['nid'])
      ->condition('cid', $component['cid']);

    switch($count_type) {
      case 'empty':
        $query->condition('data', '', '=');
        break;

      case 'notempty':
        $query->condition('data', '', '!=');
        break;
    }


    if (count($sids)) {
      $query->condition('sid', $sids, 'IN');
    }

    return $query->execute()->fetchField();
  }

}
