//  -*-C++-*-           emacs magic for .h files
/*

 $Id$

  ark -- archiver for the KDE project

  Copyright (C)

  1999: Francois-Xavier Duranceau duranceau@kde.org
  1999-2000: Corel Corporation (author: Emily Ezust, emilye@corel.com)
  2001: Corel Corporation (author: Michael Jarrett, michaelj@corel.com)
  2003: Georg Robbers <Georg.Robbers@urz.uni-hd.de>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/
#ifndef FILELISTVIEW_H
#define FILELISTVIEW_H

#include <qlistview.h>
#include <qdatetime.h>

#include <klistview.h>

class QString;
class QStringList;
class QRect;
class QPainter;
class QColorGroup;
class QMouseEvent;
class QPoint;

class KListView;

class ArkWidgetBase;

class FileListView;

enum columnName { sizeCol = 1 , packedStrCol, ratioStrCol, timeStampStrCol, otherCol };

class FileLVI : public KListViewItem
{
public:
  FileLVI(KListView* lv);

  QString fileName() const;
  long fileSize() const;
  long packedFileSize() const;
  double ratio() const;
  QDateTime timeStamp() const;

  int compare ( QListViewItem * i, int col, bool ascending ) const;
  virtual QString key(int column, bool) const;
  virtual void setText(int column, const QString &text);

private:
  bool fileIndent;
  long m_fileSize;
  long m_packedFileSize;
  double m_ratio;
  QDateTime m_timeStamp;
};


class FileListView : public KListView
{
  Q_OBJECT
public:
  FileListView(ArkWidgetBase *baseArk, QWidget* parent = 0,
	       const char* name = 0);
  FileLVI *currentItem() {return ((FileLVI *) KListView::currentItem());}
  QStringList * selectedFilenames() const;
  uint count();
  bool isSelectionEmpty();
  virtual int addColumn( const QString & label, int width = -1 );
  virtual void removeColumn( int index );
  columnName nameOfColumn( int index );

protected:
  void contentsMouseReleaseEvent(QMouseEvent *e);
  void contentsMousePressEvent(QMouseEvent *e);
  void contentsMouseMoveEvent(QMouseEvent *e);

  virtual void paintEmptyArea(QPainter * p, const QRect &rect);

private:
  QMap<int, columnName> colMap;
  int sortColumn;
  bool increasing;
  ArkWidgetBase *m_pParent;

  bool m_bPressed;
  QPoint presspos;  // this will save the click pos to correctly recognize drag events

  virtual void setSorting(int column, bool inc = TRUE);
};

#endif
