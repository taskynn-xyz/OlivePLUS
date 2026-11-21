#ifndef CHANGELOGDIALOG_H
#define CHANGELOGDIALOG_H

#include <QDialog>

/**
 * @brief The ChangelogDialog class
 *
 * Shows what OlivePLUS adds over vanilla Olive 0.1.2. It is opened from Help > Changelog and has two
 * tabs: a readable changelog grouped by area, and the full build notes the changelog was derived
 * from (BUILD-NOTES.txt, shipped inside the executable as a Qt resource so it cannot go missing).
 */
class ChangelogDialog : public QDialog
{
  Q_OBJECT
public:
  /**
   * @brief ChangelogDialog Constructor
   *
   * @param parent
   *
   * QWidget parent object. Usually this will be MainWindow.
   */
  explicit ChangelogDialog(QWidget *parent = nullptr);
};

#endif // CHANGELOGDIALOG_H
