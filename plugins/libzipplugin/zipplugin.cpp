/*
 * Copyright (c) 2007 Henrique Pinto <henrique.pinto@kdemail.net>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES ( INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION ) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * ( INCLUDING NEGLIGENCE OR OTHERWISE ) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <QString>


#ifdef LIBZIP_COMPILED_WITH_32BIT_OFF_T

#define __off_t_defined
typedef quint32 off_t;

#endif /* LIBZIP_COMPILED_WITH_32BIT_OFF_T */


#include "kerfuffle/archiveinterface.h"
#include "kerfuffle/archivefactory.h"

#include <zip.h>

#include <KLocale>
#include <KDebug>

#include <QDateTime>
#include <QFileInfo>
#include <QByteArray>
#include <QFile>
#include <QDir>

using namespace Kerfuffle;

class LibZipInterface: public ReadWriteArchiveInterface
{
	Q_OBJECT
	public:
		LibZipInterface( const QString & filename, QObject *parent )
			: ReadWriteArchiveInterface( filename, parent ), m_archive( 0 )
		{
		}

		~LibZipInterface()
		{
			kDebug( 1601 ) ;
			close();
		}

		bool open()
		{
			int errorCode;
			m_archive = zip_open( filename().toLocal8Bit(), ZIP_CREATE, &errorCode );
			if ( !m_archive )
			{
				error( i18n( "Could not open the archive '%1'", filename() ) );
				return false;
			}
			kDebug( 1601 ) << "Opened file " << filename();
			return true;
		}

		void close()
		{
			if ( m_archive )
			{
				zip_close( m_archive );
				m_archive = 0;
			}
		}

		void emitEntryForIndex( int index )
		{
			struct zip_stat stat;
			if ( zip_stat_index( m_archive, index, 0, &stat ) != 0 )
			{
				error( i18n( "An error occurred while trying to read entry #%1 of the archive", index ) );
				return;
			}

			QString filename = QDir::fromNativeSeparators(QFile::decodeName( stat.name ));

			ArchiveEntry e;

			e[ FileName ]       = filename;
			e[ InternalID ]     = filename;
			e[ CRC ]            = stat.crc;
			e[ Size ]           = static_cast<qulonglong>( stat.size );
			e[ Timestamp ]      = QDateTime::fromTime_t( stat.mtime );
			e[ CompressedSize ] = static_cast<qulonglong>( stat.comp_size );
			e[ Method ]         = stat.comp_method;
			e[ IsPasswordProtected ] = stat.encryption_method? true : false;
			e[ IsDirectory ] = (filename.right(1) == "/");

			// TODO: zip_get_file_comment returns junk sometimes, find out why
			/*
			const char *comment = zip_get_file_comment( m_archive, index, 0, 0 );
			if ( comment )
			{
				e[ Comment ] = QString( comment );
			}
			*/

			entry( e );
		}

		bool list()
		{
			kDebug( 1601 );
			if ( !open() ) // TODO: open should be called by the user, not by us
			{
				return false;
			}

			//progress( 0.0 );

			for ( int index = 0; index < zip_get_num_files( m_archive ); ++index )
			{
				emitEntryForIndex( index );
				progress( ( index+1 ) * 1.0/zip_get_num_files( m_archive ) );
			}
			close();
			return true;
		}

		QString destinationFileName( const QString& entryName, const QString& baseDir, bool preservePaths )
		{
			QString name = baseDir + '/';
			if ( preservePaths )
			{
				name += entryName;
			}
			else
			{
				name += QFileInfo( entryName ).fileName();
			}
			return name;
		}

		bool extractEntry(struct zip_file *file, QVariant entry, const QString & destinationDirectory, bool preservePaths )
		{
			if (entry.toString().right(1) == "/") { // if a folder

				//if we don't preserve paths we don't create any folders
				if (!preservePaths) return true;

				if (!QDir(destinationDirectory).mkpath(entry.toString())) {
					error( i18n( "Could not create path" ) );
					zip_fclose( file );
					return false;
				}
				zip_fclose( file );
				return true;
			}


			// 2. Open the destination file
			QFile destinationFile( destinationFileName( entry.toString(), destinationDirectory, preservePaths ) );

			//create the path if it doesn't exist already
			if (preservePaths) {
				QDir dest(destinationDirectory);
				QFileInfo fi(destinationFile.fileName());
				if (!dest.exists(fi.path())) {
					dest.mkpath(fi.path());
				}
			}

			if ( !destinationFile.open( QIODevice::WriteOnly ) )
			{
				error( i18n( "Could not write to the destination file %1, path %2", entry.toString(), destinationFile.fileName()) );
				return false;
			}

			// 3. Copy the data
			char buffer[ 65536 ];
			int readBytes = -1;
			while ( ( readBytes = zip_fread( file, &buffer, 65536 ) ) != -1 )
			{
				if ( readBytes == 0 )
				{
					break;
				}
				destinationFile.write( buffer, readBytes );
			}

			// 4. Close the files
			zip_fclose( file );
			destinationFile.close();
			return true;
		}

		bool copyFiles( const QList<QVariant> & files, const QString & destinationDirectory, Archive::CopyFlags flags )
		{
			kDebug( 1601 ) ;

			const bool preservePaths = flags & Archive::PreservePaths;

			if (!m_archive) {
				if (!open()) {
					return false;
				}
			}

			int processed = 0;
			if (!files.isEmpty()) {

				foreach( const QVariant &entry, files )
				{

					// 1. Find the entry in the archive
					struct zip_file *file = zip_fopen( m_archive, QFile::encodeName(entry.toString()), 0 );
					if ( !file )
					{
						error( i18n( "Could not locate file '%1' in the archive", entry.toString() ) );
						return false;
					}

					if (!extractEntry(file, entry, destinationDirectory, preservePaths)) {
						return false;
					}

					kDebug( 1601 ) << "Extracted " << entry.toString() ;

					progress( ( ++processed )*1.0/files.count() );
				}
			} else  {
				for ( int index = 0; index < zip_get_num_files( m_archive ); ++index )
				{

					// 1. Find the entry in the archive
					struct zip_file *file = zip_fopen_index( m_archive, index, 0 );
					if ( !file )
					{
						error( i18n( "Could not locate file #%1 in the archive", index ) );
						return false;
					}
					
					if (!extractEntry(file, QDir::fromNativeSeparators(QFile::decodeName(zip_get_name(m_archive, index, 0))), destinationDirectory, preservePaths)) {
						return false;
					}

					kDebug( 1601 ) << "Extracted entry with index" << index ;

					progress( ( index+1 ) * 1.0/zip_get_num_files( m_archive ) );
				}
			}
			close();
			return true;
		}

		bool addFiles( const QStringList & files, const CompressionOptions& options )
		{
			kDebug( 1601 ) << "adding " << files.count() << " files";

			if (!m_archive) {
				if (!open()) {
					return false;
				}
			}

			QStringList expandedFiles = files;

			kDebug( 1601 ) << "Before expanding: " << expandedFiles << QDir::currentPath();
			expandDirectories(expandedFiles);

			kDebug( 1601 ) << "After expanding: " << expandedFiles;

			QString globalWorkdir = options.value("GlobalWorkDir").toString();
			if (!globalWorkdir.isEmpty()) {
				kDebug( 1601 ) << "GlobalWorkDir is set, changing dir to " << globalWorkdir;
				QDir::setCurrent(globalWorkdir);
			}

			int processed = 0;
			foreach( const QString & file, expandedFiles )
			{

				QString relativeName = QDir::current().relativeFilePath(file);
				if (relativeName.isEmpty()) {
					//probably trying to add the current directory to the, with
					//the GlobalWorkdir set to the same value. 
					kDebug( 1601 ) << "Skipping empty relative-entry";
					continue;

				}

				if (QFileInfo(relativeName).isDir())
					if (relativeName.right(1) != "/")
						relativeName += "/";

				kDebug( 1601 ) << "Adding " << file ;

				struct zip_source *source = zip_source_file( m_archive, file.toLocal8Bit(), 0, -1 );
				if ( !source )
				{
					kDebug( 1601 ) << "Read error " << zip_strerror(m_archive);
					error( i18n( "Could not read from the input file '%1'", file ) );
					return false;
				}


				int index;
				if (  ( index = zip_add( m_archive, QFile::encodeName(relativeName), source ) ) < 0 )
				{
					error( i18n( "Could not add the file %1 to the archive.", file) );
				}

				kDebug( 1601 ) << file << " was added to the archive, index is " << index ;

				emitEntryForIndex( index );
				progress( ( ++processed )*1.0/expandedFiles.count() );
			}
			kDebug( 1601 ) << "And we're done :)" ;
			close();
			return true;
		}

		bool deleteFiles( const QList<QVariant> & files )
		{
			foreach( const QVariant& file, files )
			{
				int index = zip_name_locate( m_archive, file.toByteArray(), 0 );
				if ( index < 0 )
				{
					error( i18n( "Could not find a file named %1 in the archive.", file.toString() ) );
					return false;
				}
				zip_delete( m_archive, index );
				// TODO: emit some signal to inform the model of the deleted entry
				entryRemoved( file.toString() );
			}
			close();
			return true;
		}

	private:
		struct zip *m_archive;
};

#include "zipplugin.moc"

KERFUFFLE_PLUGIN_FACTORY( LibZipInterface )
