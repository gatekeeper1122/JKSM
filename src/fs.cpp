#include <3ds.h>
#include <string>
#include <algorithm>
#include <cstring>

#include <cstdio>

#include "fs.h"
#include "util.h"
#include "ui.h"
#include "gfx.h"
#include "data.h"

#define buff_size 100 * 1024

static FS_Archive sdmcArch, saveArch;
static FS_ArchiveID saveMode = (FS_ArchiveID)0;
static Handle fsHandle;

namespace fs
{
    void createDir(const std::string& path)
    {
        FSUSER_CreateDirectory(getSDMCArch(), fsMakePath(PATH_ASCII, path.c_str()), 0);
    }

    void init()
    {
        FSUSER_OpenArchive(&sdmcArch, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""));

        createDir("/JKSV");
        createDir("/JKSV/Saves");
        createDir("/JKSV/SysSave");
        createDir("/JKSV/ExtData");
        createDir("/JKSV/Boss");
        createDir("/JKSV/Shared");
    }

    void exit()
    {
        FSUSER_CloseArchive(sdmcArch);
        FSUSER_CloseArchive(saveArch);
    }

    void fsStartSession()
    {
        srvGetServiceHandleDirect(&fsHandle, "fs:USER");
        FSUSER_Initialize(fsHandle);
        fsUseSession(fsHandle);
    }

    void fsEndSession()
    {
        fsEndUseSession();
    }

    FS_Archive getSDMCArch()
    {
        return sdmcArch;
    }

    FS_Archive getSaveArch()
    {
        return saveArch;
    }

    FS_ArchiveID getSaveMode()
    {
        return saveMode;
    }

    bool openArchive(data::titleData& dat, const uint32_t& arch)
    {
        Result res;
        saveMode = (FS_ArchiveID)arch;

        switch(arch)
        {
            case ARCHIVE_USER_SAVEDATA:
                {
                    uint32_t path[3] = {dat.getMedia(), dat.getLow(), dat.getHigh()};
                    FS_Path binData = (FS_Path)
                    {
                        PATH_BINARY, 12, path
                    };
                    res = FSUSER_OpenArchive(&saveArch, ARCHIVE_USER_SAVEDATA, binData);
                }
                break;

            case ARCHIVE_SAVEDATA:
                {
                    res = FSUSER_OpenArchive(&saveArch, ARCHIVE_SAVEDATA, fsMakePath(PATH_EMPTY, ""));
                }
                break;

            case ARCHIVE_EXTDATA:
                {
                    uint32_t path[] = {MEDIATYPE_SD, dat.getExtData(), 0};
                    FS_Path binData = (FS_Path)
                    {
                        PATH_BINARY, 12, path
                    };
                    res = FSUSER_OpenArchive(&saveArch, ARCHIVE_EXTDATA, binData);
                }
                break;

            case ARCHIVE_SYSTEM_SAVEDATA:
                {
                    uint32_t path[2] = {MEDIATYPE_NAND, (0x00020000 | dat.getUnique())};
                    FS_Path binData = {PATH_BINARY, 8, path};
                    res = FSUSER_OpenArchive(&saveArch, ARCHIVE_SYSTEM_SAVEDATA, binData);
                }
                break;

            case ARCHIVE_BOSS_EXTDATA:
                {
                    uint32_t path[3] = {MEDIATYPE_SD, dat.getExtData(), 0};
                    FS_Path binData = {PATH_BINARY, 12, path};
                    res = FSUSER_OpenArchive(&saveArch, ARCHIVE_BOSS_EXTDATA, binData);
                }
                break;

            case ARCHIVE_SHARED_EXTDATA:
                {
                    uint32_t path[3] = {MEDIATYPE_NAND, dat.getExtData(), 0x00048000};

                    FS_Path binPath  = {PATH_BINARY, 0xC, path};

                    res = FSUSER_OpenArchive(&saveArch, ARCHIVE_SHARED_EXTDATA, binPath);
                }
                break;
        }

        return R_SUCCEEDED(res);
    }

    void commitData(const uint32_t& mode)
    {
        if(mode != ARCHIVE_EXTDATA && mode != ARCHIVE_BOSS_EXTDATA)
        {
            Result res = FSUSER_ControlArchive(saveArch, ARCHIVE_ACTION_COMMIT_SAVE_DATA, NULL, 0, NULL, 0);
            if(res)
                ui::showMessage("Failed to commit save data!");
        }
    }

    void deleteSv(const uint32_t& mode)
    {
        if(data::haxMode)
        {
            fs::fsEndSession();
        }

        if(mode != ARCHIVE_EXTDATA && mode != ARCHIVE_BOSS_EXTDATA)
        {
            u64 in = ((u64)SECUREVALUE_SLOT_SD << 32) | (data::curData.getUnique() << 8);
            u8 out;

            Result res = FSUSER_ControlSecureSave(SECURESAVE_ACTION_DELETE, &in, 8, &out, 1);
            if(res)
            {
                ui::showMessage("Failed to delete secure value");
            }
        }

        if(data::haxMode)
        {
            fs::fsStartSession();
        }
    }

    fsfile::fsfile(const FS_Archive& _arch, const std::string& _path, const uint32_t& openFlags)
    {
        error = FSUSER_OpenFile(&fHandle, _arch, fsMakePath(PATH_ASCII, _path.data()), openFlags, 0);
        if(error)
            open = false;
        else
        {
            FSFILE_GetSize(fHandle, &fSize);
            open = true;
        }
    }

    fsfile::fsfile(const FS_Archive& _arch, const std::string& _path, const uint32_t& openFlags, const uint64_t& crSize)
    {
        error = FSUSER_CreateFile(_arch, fsMakePath(PATH_ASCII, _path.data()), 0, crSize);
        if(error == 0)
        {
            error = FSUSER_OpenFile(&fHandle, _arch, fsMakePath(PATH_ASCII, _path.data()), openFlags, 0);
            if(error)
                open = false;
            else
            {
                FSFILE_GetSize(fHandle, &fSize);
                open = true;
            }
        }
    }

    fsfile::fsfile(const FS_Archive& _arch, const std::u16string& _path, const uint32_t& openFlags)
    {
        error = FSUSER_OpenFile(&fHandle, _arch, fsMakePath(PATH_UTF16, _path.data()), openFlags, 0);
        if(error)
            open = false;
        else
        {
            FSFILE_GetSize(fHandle, &fSize);
            open = true;
        }
    }

    fsfile::fsfile(const FS_Archive& _arch, const std::u16string& _path, const uint32_t& openFlags, const uint64_t& crSize)
    {
        error = FSUSER_OpenFile(&fHandle, _arch, fsMakePath(PATH_UTF16, _path.data()), FS_OPEN_WRITE, 0);
        if(error)
        {
            error = FSUSER_CreateFile(_arch, fsMakePath(PATH_UTF16, _path.data()), 0, crSize);
            if(error == 0)
            {
                error = FSUSER_OpenFile(&fHandle, _arch, fsMakePath(PATH_UTF16, _path.data()), openFlags, 0);
                if(error)
                    open = false;
                else
                {
                    FSFILE_GetSize(fHandle, &fSize);
                    open = true;
                }
            }
        }
        else
        {
            open = true;
        }
    }

    fsfile::~fsfile()
    {
        FSFILE_Close(fHandle);
    }

    void fsfile::read(uint8_t *buf, uint32_t& readOut, const uint32_t& max)
    {
        Result res = FSFILE_Read(fHandle, &readOut, offset, buf, max);
        if(R_FAILED(res))
        {
            if(readOut > max)
                readOut = max;

            std::memset(buf, 0x00, max);
        }

        offset += readOut;
    }

    void fsfile::write(const uint8_t* buf, uint32_t& written, const uint32_t& size)
    {
        FSFILE_Write(fHandle, &written, offset, buf, size, FS_WRITE_FLUSH);

        offset += written;
    }

    void fsfile::writeString(const std::string& str)
    {
        uint32_t written = 0;
        FSFILE_Write(fHandle, &written, offset, str.c_str(), str.length(), FS_WRITE_FLUSH);

        offset += written;
    }

    uint8_t fsfile::getByte()
    {
        uint8_t ret = 0;
        FSFILE_Read(fHandle, NULL, offset, &ret, 1);
        ++offset;
        return ret;
    }

    void fsfile::putByte(const uint8_t& put)
    {
        FSFILE_Write(fHandle, NULL, offset, &put, 1, FS_WRITE_FLUSH);
        ++offset;
    }

    bool fsfile::eof()
    {
        return offset < fSize ? false : true;
    }

    void fsfile::seek(const int& pos, const uint8_t& seekFrom)
    {
        switch(seekFrom)
        {
            case seek_beg:
                offset = pos;
                break;

            case seek_cur:
                offset += pos;
                break;

            case seek_end:
                offset = fSize + pos;
                break;
        }
    }

    uint64_t fsfile::getSize()
    {
        return fSize;
    }

    uint32_t fsfile::getError()
    {
        return error;
    }

    uint64_t fsfile::getOffset()
    {
        return offset;
    }

    bool fsfile::isOpen()
    {
        return open;
    }

    struct
    {
        bool operator()(const FS_DirectoryEntry& a, const FS_DirectoryEntry& b)
        {
            for(unsigned i = 0; i < 0x106; i++)
            {
                int charA = std::tolower(a.name[i]), charB = std::tolower(b.name[i]);
                if(charA != charB)
                    return charA < charB;
            }

            return false;
        }
    } sortDirs;

    dirList::dirList(const FS_Archive& arch, const std::u16string& p)
    {
        a = arch;

        path = p;

        FSUSER_OpenDirectory(&d, a, fsMakePath(PATH_UTF16, p.data()));

        uint32_t read = 0;
        do
        {
            FS_DirectoryEntry get;
            FSDIR_Read(d, &read, 1, &get);
            if(read == 1)
                entry.push_back(get);
        }
        while(read > 0);

        FSDIR_Close(d);

        std::sort(entry.begin(), entry.end(), sortDirs);
    }

    dirList::~dirList()
    {
        entry.clear();
    }

    void dirList::rescan()
    {
        entry.clear();

        FSUSER_OpenDirectory(&d, a, fsMakePath(PATH_UTF16, path.data()));

        uint32_t read = 0;
        do
        {
            FS_DirectoryEntry ent;
            FSDIR_Read(d, &read, 1, &ent);

            if(read == 1)
                entry.push_back(ent);
        }
        while(read > 0);

        FSDIR_Close(d);

        std::sort(entry.begin(), entry.end(), sortDirs);
    }

    void dirList::reassign(const std::u16string& p)
    {
        entry.clear();

        path = p;

        FSUSER_OpenDirectory(&d, a, fsMakePath(PATH_UTF16, path.data()));
        uint32_t read = 0;
        do
        {
            FS_DirectoryEntry ent;
            FSDIR_Read(d, &read, 1, &ent);

            if(read == 1)
                entry.push_back(ent);
        }
        while(read > 0);

        FSDIR_Close(d);

        std::sort(entry.begin(), entry.end(), sortDirs);
    }

    const uint32_t dirList::getCount()
    {
        return entry.size();
    }

    bool dirList::isDir(unsigned i)
    {
        return entry[i].attributes == FS_ATTRIBUTE_DIRECTORY;
    }

    const std::u16string dirList::getItem(unsigned i)
    {
        return std::u16string((char16_t *)entry[i].name);
    }

    void copyFileToSD(const FS_Archive& arch, const std::u16string& from, const std::u16string& to)
    {
        fsfile in(arch, from, FS_OPEN_READ);
        fsfile out(getSDMCArch(), to, FS_OPEN_WRITE | FS_OPEN_CREATE);

        if(!in.isOpen())
        {
            ui::showMessage("There was an error opening the\n file for reading.");
            return;
        }
        else if(!out.isOpen())
        {
            ui::showMessage("There was an error opening the\n file for writing.");
            return;
        }

        uint8_t *buff = new uint8_t[buff_size];
        std::string copyString = util::getWrappedString("Copying \n" + util::toUtf8(from) + "...", 224);
        ui::progressBar prog((uint32_t)in.getSize());
        do
        {
            uint32_t read, written;
            in.read(buff, read, buff_size);
            out.write(buff, written, read);

            if(written != read)
            {
                ui::showMessage("Size mismatch.");
            }

            prog.update((uint32_t)in.getOffset());

            gfx::frameBegin();
            gfx::frameStartTop();
            ui::drawTopBar("Dumping...");
            gfx::frameStartBot();
            prog.draw(copyString);
            gfx::frameEnd();
        }
        while(!in.eof());

        delete[] buff;
    }

    void copyDirToSD(const FS_Archive& arch, const std::u16string& from, const std::u16string& to)
    {
        dirList list(arch, from);

        for(unsigned i = 0; i < list.getCount(); i++)
        {
            if(list.isDir(i))
            {
                std::u16string newFrom = from + list.getItem(i) + util::toUtf16("/");
                std::u16string newTo = to + list.getItem(i);
                FSUSER_CreateDirectory(getSDMCArch(), fsMakePath(PATH_UTF16, newTo.data()), 0);
                newTo += util::toUtf16("/");

                copyDirToSD(arch, newFrom, newTo);
            }
            else
            {
                std::u16string fullFrom = from + list.getItem(i);
                std::u16string fullTo   = to   + list.getItem(i);

                copyFileToSD(arch, fullFrom, fullTo);
            }
        }
    }

    void backupArchive(const std::u16string& outpath)
    {
        std::u16string pathIn = util::toUtf16("/");
        copyDirToSD(saveArch, pathIn, outpath);
    }

    void copyFileToArch(const FS_Archive& arch, const std::u16string& from, const std::u16string& to)
    {
        fsfile in(getSDMCArch(), from, FS_OPEN_READ);
        fsfile out(arch, to, FS_OPEN_WRITE, in.getSize());

        if(!in.isOpen())
        {
            ui::showMessage("There was an error opening the\n file for reading.");
            return;
        }
        else if(!out.isOpen())
        {
            ui::showMessage("There was an error opening the\n file for writing.");

            char tmp[16];
            std::sprintf(tmp, "0x%08X", (unsigned)out.getError());
            ui::showMessage(tmp);

            return;
        }

        uint8_t *buff = new uint8_t[buff_size];
        std::string copyString = util::getWrappedString("Copying \n" + util::toUtf8(from) + "...", 224);
        ui::progressBar prog((uint32_t)in.getSize());
        do
        {
            uint32_t read, written;
            in.read(buff, read, buff_size);
            out.write(buff, written, read);

            prog.update((uint64_t)in.getOffset());

            gfx::frameBegin();
            gfx::frameStartTop();
            ui::drawTopBar("Restoring...");
            gfx::frameStartBot();
            prog.draw(copyString);
            gfx::frameEnd();
        }
        while(!in.eof());

        delete[] buff;
    }

    void copyDirToArch(const FS_Archive& arch, const std::u16string& from, const std::u16string& to)
    {
        dirList dir(getSDMCArch(), from);

        for(unsigned i = 0; i < dir.getCount(); i++)
        {
            if(dir.isDir(i))
            {
                std::u16string newFrom = from + dir.getItem(i) + util::toUtf16("/");

                std::u16string newTo   = to + dir.getItem(i);
                FSUSER_CreateDirectory(arch, fsMakePath(PATH_UTF16, newTo.data()), 0);
                newTo += util::toUtf16("/");

                copyDirToArch(arch, newFrom, newTo);
            }
            else
            {
                std::u16string sdPath = from + dir.getItem(i);
                std::u16string archPath = to + dir.getItem(i);

                copyFileToArch(arch, sdPath, archPath);
            }
        }
    }

    void restoreToArchive(const std::u16string& inpath)
    {
        std::u16string root = util::toUtf16("/");
        FSUSER_DeleteDirectoryRecursively(saveArch, fsMakePath(PATH_UTF16, root.data()));
        copyDirToArch(saveArch, inpath, root);
        commitData(saveMode);
        deleteSv(saveMode);
    }

    void backupAll()
    {
        ui::progressBar prog(data::titles.size());
        for(unsigned i = 0; i < data::titles.size(); i++)
        {
            std::string copyStr = "Working on \"" + util::toUtf8(data::titles[i].getTitle()) + "\"...";
            prog.update(i);

            //Sue me
            gfx::frameBegin();
            gfx::frameStartBot();
            prog.draw(copyStr);
            gfx::frameEnd();

            if(fs::openArchive(data::titles[i], ARCHIVE_USER_SAVEDATA))
            {
                util::createTitleDir(data::titles[i], ARCHIVE_USER_SAVEDATA);

                std::u16string outpath = util::createPath(data::titles[i], ARCHIVE_USER_SAVEDATA) + util::getDateString();
                FSUSER_CreateDirectory(fs::getSDMCArch(), fsMakePath(PATH_UTF16, outpath.data()), 0);
                outpath += util::toUtf16("/");

                backupArchive(outpath);

                FSUSER_CloseArchive(fs::getSaveArch());
            }

            if(fs::openArchive(data::titles[i], ARCHIVE_EXTDATA))
            {
                util::createTitleDir(data::titles[i], ARCHIVE_EXTDATA);

                std::u16string outpath = util::createPath(data::titles[i], ARCHIVE_EXTDATA) + util::getDateString();
                FSUSER_CreateDirectory(fs::getSDMCArch(), fsMakePath(PATH_UTF16, outpath.data()), 0);
                outpath += util::toUtf16("/");

                backupArchive(outpath);

                FSUSER_CloseArchive(fs::getSaveArch());
            }
        }
    }
}
