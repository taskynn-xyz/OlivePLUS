#include "styling.h"

#include "global/config.h"

bool olive::styling::UseDarkIcons()
{
  return olive::CurrentConfig.style == kOliveDefaultLight || olive::CurrentConfig.style == kNativeDarkIcons;
}

QColor olive::styling::GetIconColor()
{
  if (UseDarkIcons()) {
    return Qt::black;
  } else {
    return Qt::white;
  }
}



bool olive::styling::UseNativeUI()
{
  return olive::CurrentConfig.style == kNativeLightIcons || olive::CurrentConfig.style == kNativeDarkIcons;
}

QString olive::styling::GetFluentStyleSheet(bool dark)
{
  // Windows 11 Fluent-ish palette. The accent is Microsoft's #0078D4, lightened for dark surfaces so
  // selected text stays readable.
  const QString window = dark ? "#202020" : "#f3f3f3";
  const QString surface = dark ? "#272727" : "#fbfbfb";
  const QString field = dark ? "#2b2b2b" : "#ffffff";
  const QString border = dark ? "#3d3d3d" : "#d6d6d6";
  const QString hover = dark ? "#343434" : "#e8e8e8";
  const QString pressed = dark ? "#1c1c1c" : "#dedede";
  const QString text = dark ? "#f0f0f0" : "#1b1b1b";
  const QString disabled_text = dark ? "#7a7a7a" : "#9a9a9a";
  const QString accent = dark ? "#4cc2ff" : "#0078d4";
  const QString accent_solid = "#0078d4";
  const QString accent_text = "#ffffff";
  const QString selection = dark ? "#005a9e" : "#cce4f7";
  const QString selection_text = dark ? "#ffffff" : "#000000";

  QString sheet;

  // --- containers ---------------------------------------------------------
  sheet += QString(
    "QMainWindow, QDialog, QDockWidget, QWidget#qt_scrollarea_viewport { background: %1; color: %2; }"
    "QGroupBox { border: 1px solid %3; border-radius: 6px; margin-top: 12px; padding: 8px 6px 6px 6px; }"
    "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; left: 10px; padding: 0 4px; }"
    "QStatusBar { background: %4; border-top: 1px solid %3; }"
    "QToolTip { background: %4; color: %2; border: 1px solid %3; border-radius: 4px; padding: 4px 6px; }"
    ).arg(window, text, border, surface);

  // --- panel tabs and dock titles -----------------------------------------
  sheet += QString(
    "QDockWidget::title { background: %1; padding: 6px 8px; border-bottom: 1px solid %2; }"
    "QTabBar::tab { background: transparent; color: %3; padding: 6px 14px; margin-right: 2px;"
    "  border: none; border-top-left-radius: 6px; border-top-right-radius: 6px; }"
    "QTabBar::tab:hover { background: %4; }"
    "QTabBar::tab:selected { background: %5; border-bottom: 2px solid %6; }"
    ).arg(surface, border, text, hover, window, accent);

  // --- buttons ------------------------------------------------------------
  sheet += QString(
    "QPushButton { background: %1; color: %2; border: 1px solid %3; border-radius: 4px;"
    "  padding: 5px 14px; min-height: 18px; }"
    "QPushButton:hover { background: %4; border-color: %3; }"
    "QPushButton:pressed { background: %5; }"
    "QPushButton:checked { background: %6; border-color: %6; color: %7; }"
    "QPushButton:disabled { background: %1; color: %8; border-color: %3; }"
    "QToolButton { background: transparent; border: 1px solid transparent; border-radius: 4px; padding: 3px; }"
    "QToolButton:hover { background: %4; }"
    "QToolButton:pressed { background: %5; }"
    "QToolButton:checked { background: %6; border-color: %6; }"
    ).arg(surface, text, border, hover, pressed, accent_solid, accent_text, disabled_text);

  // --- text inputs --------------------------------------------------------
  sheet += QString(
    "QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QAbstractSpinBox, QTextEdit, QPlainTextEdit {"
    "  background: %1; color: %2; border: 1px solid %3; border-radius: 4px; padding: 3px 6px;"
    "  selection-background-color: %4; selection-color: %5; }"
    "QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus, QTextEdit:focus {"
    "  border: 1px solid %6; }"
    "QComboBox::drop-down, QAbstractSpinBox::up-button, QAbstractSpinBox::down-button { border: none; }"
    "QComboBox QAbstractItemView { background: %1; color: %2; border: 1px solid %3;"
    "  selection-background-color: %4; selection-color: %5; outline: 0; }"
    ).arg(field, text, border, selection, selection_text, accent);

  // --- menus --------------------------------------------------------------
  sheet += QString(
    "QMenuBar { background: %1; }"
    "QMenuBar::item { background: transparent; padding: 5px 10px; border-radius: 4px; }"
    "QMenuBar::item:selected { background: %2; }"
    "QMenu { background: %3; color: %4; border: 1px solid %5; border-radius: 6px; padding: 4px; }"
    "QMenu::item { padding: 6px 26px 6px 22px; border-radius: 4px; }"
    "QMenu::item:selected { background: %6; color: %7; }"
    "QMenu::separator { height: 1px; background: %5; margin: 4px 10px; }"
    ).arg(window, hover, surface, text, border, accent_solid, accent_text);

  // --- scrollbars ---------------------------------------------------------
  sheet += QString(
    "QScrollBar:vertical { background: transparent; width: 11px; margin: 0; }"
    "QScrollBar::handle:vertical { background: %1; border-radius: 4px; min-height: 28px; margin: 2px; }"
    "QScrollBar::handle:vertical:hover { background: %2; }"
    "QScrollBar:horizontal { background: transparent; height: 11px; margin: 0; }"
    "QScrollBar::handle:horizontal { background: %1; border-radius: 4px; min-width: 28px; margin: 2px; }"
    "QScrollBar::handle:horizontal:hover { background: %2; }"
    "QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }"
    "QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }"
    ).arg(dark ? "#4a4a4a" : "#c2c2c2", dark ? "#666666" : "#a8a8a8");

  // --- progress, checkboxes, sliders --------------------------------------
  sheet += QString(
    "QProgressBar { background: %1; border: 1px solid %2; border-radius: 4px; text-align: center;"
    "  min-height: 16px; color: %3; }"
    "QProgressBar::chunk { background: %4; border-radius: 3px; }"
    "QCheckBox::indicator { width: 14px; height: 14px; border: 1px solid %2; border-radius: 3px; background: %1; }"
    "QCheckBox::indicator:checked { background: %4; border-color: %4; }"
    "QSlider::groove:horizontal { height: 4px; background: %2; border-radius: 2px; }"
    "QSlider::sub-page:horizontal { background: %4; border-radius: 2px; }"
    "QSlider::handle:horizontal { width: 14px; margin: -6px 0; border-radius: 7px; background: %5; }"
    ).arg(field, border, text, accent_solid, accent);

  // --- item views ---------------------------------------------------------
  sheet += QString(
    "QTreeView, QListView, QTableView, QHeaderView { background: %1; color: %2; border: 1px solid %3;"
    "  border-radius: 4px; alternate-background-color: %4; }"
    "QTreeView::item:selected, QListView::item:selected, QTableView::item:selected {"
    "  background: %5; color: %6; }"
    "QHeaderView::section { background: %4; color: %2; border: none; border-right: 1px solid %3;"
    "  border-bottom: 1px solid %3; padding: 4px 6px; }"
    ).arg(window, text, border, surface, selection, selection_text);

  return sheet;
}
