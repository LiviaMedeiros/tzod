#pragma once
#include "inc/fs/FileSystem.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace FS {

class OSFileSystem : public FileSystem
{
	struct AutoHandle
	{
		HANDLE h;
		AutoHandle() : h(nullptr) {}
		~AutoHandle()
		{
			if( nullptr != h && INVALID_HANDLE_VALUE != h )
			{
				CloseHandle(h);
			}
		}
	private:
		AutoHandle(const AutoHandle&);
		AutoHandle& operator = (const AutoHandle&);
	};

	class OSFile
		: public File
		, public std::enable_shared_from_this<OSFile>
	{
	public:
		OSFile(std::wstring fileName, FileMode mode);
		virtual ~OSFile();

		// File
		virtual std::shared_ptr<MemMap> QueryMap();
		virtual std::shared_ptr<Stream> QueryStream();
		virtual void Unmap();
		virtual void Unstream();

	private:
		class OSMemMap : public MemMap
		{
		public:
			OSMemMap(std::shared_ptr<OSFile> parent, HANDLE hFile);
			virtual ~OSMemMap();

			virtual char* GetData();
			virtual unsigned long GetSize() const;
			virtual void SetSize(unsigned long size); // may invalidate pointer returned by GetData()

		private:
			std::shared_ptr<OSFile> _file;
			HANDLE _hFile;
			AutoHandle _map;
			void *_data;
			size_t _size;
			void SetupMapping();
		};

		class OSStream : public Stream
		{
		public:
			OSStream(std::shared_ptr<OSFile> parent, HANDLE hFile);
			virtual ~OSStream();

			virtual size_t Read(void *dst, size_t size, size_t count);
			virtual void Write(const void *src, size_t size);
			virtual void Seek(long long amount, unsigned int origin);
			virtual long long Tell() const;

		private:
			std::shared_ptr<OSFile> _file;
			HANDLE _hFile;
		};

	private:
		AutoHandle _file;
		FileMode _mode;
		bool _mapped;
		bool _streamed;
	};

	std::wstring  _rootDirectory;

	virtual std::shared_ptr<File> RawOpen(const std::string &fileName, FileMode mode);

public:
	explicit OSFileSystem(std::wstring &&rootDirectory);

	std::shared_ptr<FileSystem> GetFileSystem(const std::string &path, bool create = false, bool nothrow = false) override;
	std::vector<std::string> EnumAllFiles(std::string_view mask) override;

	static std::shared_ptr<OSFileSystem> Create(const std::string &rootDirectory);
};

} // namespace FS
