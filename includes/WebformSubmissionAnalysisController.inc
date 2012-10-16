<?php

/**
 * @file Defines the WebformSubmissionAnalysisController interface.
 */

interface WebformSubmissionAnalysisController {

  public function getComponentSubmissionMin($component);
  public function getComponentSubmissionMax($component);
  public function getComponentSubmissionAvg($component);
  public function getComponentSubmissionCount($component, $count_type);

}
