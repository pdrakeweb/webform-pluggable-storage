/**
 * @file Defines the webformSubmissionControllerMysql class.
 */

class webformSubmissionControllerMysql extends webformSubmissionController {

  function __construct() {
  }

  public function loadSubmissions($filters = array(), $header = NULL, $pager_count = 0) {
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
        $this->submissions[$row->sid] = FALSE;
      }
    }

    // If there are no submissions being retrieved, return an empty array.
    if (empty($sids)) {
      return $this->submissions;
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
        $this->submissions[$row->sid] = new stdClass();
        $this->submissions[$row->sid]->sid = $row->sid;
        $this->submissions[$row->sid]->nid = $row->nid;
        $this->submissions[$row->sid]->submitted = $row->submitted;
        $this->submissions[$row->sid]->remote_addr = $row->remote_addr;
        $this->submissions[$row->sid]->uid = $row->uid;
        $this->submissions[$row->sid]->name = $row->name;
        $this->submissions[$row->sid]->is_draft = $row->is_draft;
        $this->submissions[$row->sid]->data = array();
      }
      // CID may be NULL if this submission does not actually contain any data.
      if ($row->cid) {
        $this->submissions[$row->sid]->data[$row->cid][$row->no] = $row->data;
      }
      $previous = $row->sid;
    }

    foreach (module_implements('webform_submission_load') as $module) {
      $function = $module . '_webform_submission_load';
      $function($this->submissions);
    }
  }

  public static function getSubmissionCount($nid, $uid = NULL, $reset = FALSE) {
    static $counts;

    if (!isset($counts[$nid][$uid]) || $reset) {
      $query = db_select('webform_submissions', 'ws')
        ->addTag('webform_get_submission_count')
        ->condition('ws.nid', $nid)
        ->condition('ws.is_draft', 0);
      $arguments = array($nid);
      if ($uid !== NULL) {
        $query->condition('ws.uid', $uid);
      }
      if ($uid === 0) {
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

      $counts[$nid][$uid] = $query->countQuery()->execute()->fetchField();
    }
    return $counts[$nid][$uid];
  }
}