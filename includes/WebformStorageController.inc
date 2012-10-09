<?php

/**
 * @file Defines the WebformStorageController factory class.
 *
 */

class WebformStorageController {

  private static $storage_controllers;

  /**
   * Creates and returns an instance of the configured webform submission
   * storage controller.
   *
   * @param string $context
   * Contains the context for which the storage controller is requested.
   *
   * @return object $storage_controller
   * Webform storage controller object.
   */
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

  /**
   * Obtains the type of the configured webform submission controller.
   *
   * @param string $context
   * Contains the context for which the storage controller is requested.
   *
   * @return string $controller_type
   * Text representation of the storage controller type.
   */
  public static function getInstanceType($context = 'submission') {
    $storage_controller = self::getInstance($context);
    return $storage_controller->controller_type;
  }

}
