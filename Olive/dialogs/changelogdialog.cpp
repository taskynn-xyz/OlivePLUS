#include "changelogdialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QTextBrowser>
#include <QDialogButtonBox>
#include <QFile>
#include <QFontDatabase>
#include <QTextStream>

#include "global/global.h"

namespace {

/// Load a file from the executable's Qt resources, falling back to a readable message
QString read_resource(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return QString();
  }

  QTextStream stream(&file);
  return stream.readAll();
}

}

ChangelogDialog::ChangelogDialog(QWidget *parent) :
  QDialog(parent)
{
  setWindowTitle(QString("%1 - Changelog").arg(olive::AppName));
  resize(820, 640);

  QVBoxLayout* layout = new QVBoxLayout(this);

  QTabWidget* tabs = new QTabWidget(this);

  // Tab 1: the readable changelog (rich text)
  QTextBrowser* changelog = new QTextBrowser(tabs);
  changelog->setOpenExternalLinks(true);

  const QString changelog_html = read_resource(":/changelog/changelog.html");
  if (changelog_html.isEmpty()) {
    changelog->setPlainText(tr("The changelog could not be loaded."));
  } else {
    changelog->setHtml(changelog_html);
  }
  tabs->addTab(changelog, tr("Changelog"));

  // Tab 2: the raw build notes the changelog is summarised from
  QTextBrowser* notes = new QTextBrowser(tabs);
  notes->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  notes->setLineWrapMode(QTextEdit::NoWrap);

  const QString build_notes = read_resource(":/changelog/buildnotes.txt");
  if (build_notes.isEmpty()) {
    notes->setPlainText(tr("The build notes could not be loaded."));
  } else {
    notes->setPlainText(build_notes);
  }
  tabs->addTab(notes, tr("Build Notes"));

  layout->addWidget(tabs);

  QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  layout->addWidget(buttons);

  connect(buttons, SIGNAL(rejected()), this, SLOT(reject()));
}
