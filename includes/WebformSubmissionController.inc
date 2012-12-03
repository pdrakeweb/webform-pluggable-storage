<?php

/**
 * @file Defines the WebformSubmissionController interface.
 */

interface WebformSubmissionController {

  public function deleteComponentSubmissionData($component);
  public function deleteSubmissions($node, $submission = FALSE);
  public function getComponentSubmissionData($component, $sids = array(), $fields = array('no', 'data'));
  public function getDraftSubmissionID($nid, $uid);
  public function getNextSubmissionID($node, $submission);
  public function getPreviousSubmissionID($node, $submission);
  public function getSubmissionCount($nid, $uid = NULL, $submission_interval = -1, $reset = FALSE);
  public function getSubmissions($filters = array(), $header = NULL, $pager_from = NULL, $pager_count = 0);
  public function insertSubmission($node, $submission);
  public function updateSubmission($node, $submission);
  public function validateComponentSubmissionUnique($nid, $cid, $value, $sid = FALSE);

}
