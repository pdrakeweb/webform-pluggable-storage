<?php

/**
 * @file Defines the WebformStorageController class.
 *
 */

class WebformStorageController {

  private static $storage_controllers;

  public static function getInstance($context = 'submission') {
    if (isset(self::$storage_controllers[$context])) {
      return self::$storage_controllers[$context];
    }

    ctools_include('plugins');
    $registry = webform_controllers();
    $info = $registry[$context];
    $plugins = ctools_get_plugins('webform', 'plugins');
    if (isset($plugins[$info['plugin']]) && $class = ctools_plugin_get_class($plugins[$info['plugin']], 'handler')) {
      self::$storage_controllers[$context] = new $class();
      return self::$storage_controllers[$context];
    }
    else {
      throw new ErrorException('Unable to instantiate storage controller class for context: ' . $context);
    }
  }
}
