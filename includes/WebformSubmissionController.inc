<?php

/**
 * @file Defines the WebformSubmissionController interface.
 */

interface WebformSubmissionController {

  public function deleteComponentSubmissionData($component);
  public function deleteSubmissions($node, $submission);
  public function getComponentSubmissionData($component, $sids = array(), $fields = array('no', 'data'));
  public function getDraftSubmissionID($nid, $uid);
  public function getNextSubmissionID($node, $submission);
  public function getPreviousSubmissionID($node, $submission);
  public function getSubmissionCount($nid, $uid = NULL, $reset = FALSE);
  public function getSubmissions($filters = array(), $header = NULL, $pager_count = 0);
  public function insertSubmission($node, $submission);
  public function updateSubmission($node, $submission);
  public function validateComponentSubmissionUnique($nid, $cid, $value, $sid = FALSE);

}
