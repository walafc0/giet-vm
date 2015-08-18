///////////////////////////////////////////////////////////////////////////////////////
// File   : main.c   (for shell application)
// Date   : july 2015
// author : Clément Guérin
///////////////////////////////////////////////////////////////////////////////////////
// Simple shell for GIET_VM.
///////////////////////////////////////////////////////////////////////////////////////

#include "stdio.h"
#include "string.h"
#include "malloc.h"

#define BUF_SIZE    (256)
#define MAX_ARGS    (32)

struct command_t
{
    char *name;
    void (*fn)(int, char**);
};

////////////////////////////////////////////////////////////////////////////////
//  Shell  Commands
////////////////////////////////////////////////////////////////////////////////

struct command_t cmd[];

///////////////////////////////////////////
static void cmd_help(int argc, char** argv)
{
    int i;

    giet_tty_printf("available commands:\n");

    for (i = 0; cmd[i].name; i++)
    {
        giet_tty_printf("\t%s\n", cmd[i].name);
    }
}

///////////////////////////////////////////////
static void cmd_proctime(int argc, char** argv)
{
    giet_tty_printf("%u\n", giet_proctime());
}

/////////////////////////////////////////
static void cmd_ls(int argc, char** argv)
{
    int fd;
    fat_dirent_t entry;

    if (argc < 2)
        fd = giet_fat_opendir("/");
    else
        fd = giet_fat_opendir(argv[1]);

    if (fd < 0)
    {
        giet_tty_printf("can't list directory (err=%d)\n", fd);
        return;
    }

    while (giet_fat_readdir(fd, &entry) == 0)
    {
        if (entry.is_dir)
            giet_tty_printf("dir ");
        else
            giet_tty_printf("file");

        giet_tty_printf(" | size = %d \t| cluster = %X \t| %s\n",
                        entry.size, entry.cluster, entry.name );
    }

    giet_fat_closedir(fd);
}

////////////////////////////////////////////
static void cmd_mkdir(int argc, char** argv)
{
    if (argc < 2)
    {
        giet_tty_printf("%s <path>\n", argv[0]);
        return;
    }

    int ret = giet_fat_mkdir(argv[1]);
    if (ret < 0)
    {
        giet_tty_printf("can't create directory (err=%d)\n", ret);
    }
}

/////////////////////////////////////////
static void cmd_cp(int argc, char** argv)
{
    if (argc < 3)
    {
        giet_tty_printf("%s <src> <dst>\n", argv[0]);
        return;
    }

    char buf[1024];
    int src_fd = -1;
    int dst_fd = -1;
    fat_file_info_t info;
    int size;
    int i;

    src_fd = giet_fat_open( argv[1] , O_RDONLY );
    if (src_fd < 0)
    {
        giet_tty_printf("can't open %s (err=%d)\n", argv[1], src_fd);
        goto exit;
    }

    giet_fat_file_info(src_fd, &info);
    if (info.is_dir)
    {
        giet_tty_printf("can't copy a directory\n");
        goto exit;
    }
    size = info.size;

    dst_fd = giet_fat_open( argv[2] , O_CREATE | O_TRUNC );
    if (dst_fd < 0)
    {
        giet_tty_printf("can't open %s (err=%d)\n", argv[2], dst_fd);
        goto exit;
    }

    giet_fat_file_info(dst_fd, &info);
    if (info.is_dir)
    {
        giet_tty_printf("can't copy to a directory\n"); // TODO
        goto exit;
    }

    i = 0;
    while (i < size)
    {
        int len = (size - i < 1024 ? size - i : 1024);
        int wlen;

        giet_tty_printf("\rwrite %d/%d (%d%%)", i, size, 100*i/size);

        len = giet_fat_read(src_fd, &buf, len);
        wlen = giet_fat_write(dst_fd, &buf, len);
        if (wlen != len)
        {
            giet_tty_printf("\nwrite error\n");
            goto exit;
        }
        i += len;
    }
    giet_tty_printf("\n");

exit:
    if (src_fd >= 0)
        giet_fat_close(src_fd);
    if (dst_fd >= 0)
        giet_fat_close(dst_fd);
}

/////////////////////////////////////////
static void cmd_rm(int argc, char **argv)
{
    if (argc < 2)
    {
        giet_tty_printf("%s <file>\n", argv[0]);
        return;
    }

    int ret = giet_fat_remove(argv[1], 0);
    if (ret < 0)
    {
        giet_tty_printf("can't remove %s (err=%d)\n", argv[1], ret);
    }
}

////////////////////////////////////////////
static void cmd_rmdir(int argc, char **argv)
{
    if (argc < 2)
    {
        giet_tty_printf("%s <path>\n", argv[0]);
        return;
    }

    int ret = giet_fat_remove(argv[1], 1);
    if (ret < 0)
    {
        giet_tty_printf("can't remove %s (err=%d)\n", argv[1], ret);
    }
}

/////////////////////////////////////////
static void cmd_mv(int argc, char **argv)
{
    if (argc < 3)
    {
        giet_tty_printf("%s <src> <dst>\n", argv[0]);
        return;
    }

    int ret = giet_fat_rename(argv[1], argv[2]);
    if (ret < 0)
    {
        giet_tty_printf("can't move %s to %s (err=%d)\n", argv[1], argv[2], ret);
    }
}

///////////////////////////////////////////
static void cmd_exec(int argc, char **argv)
{
    if (argc < 2)
    {
        giet_tty_printf("%s <pathname>\n", argv[0]);
        return;
    }

    int ret = giet_exec_application(argv[1]);
    if ( ret == -1 )
    {
        giet_tty_printf("\n  error : %s not found\n", argv[1] );
    }
}

///////////////////////////////////////////
static void cmd_kill(int argc, char **argv)
{
    if (argc < 2)
    {
        giet_tty_printf("%s <pathname>\n", argv[0]);
        return;
    }

    int ret = giet_kill_application(argv[1]);
    if ( ret == -1 )
    {
        giet_tty_printf("\n  error : %s not found\n", argv[1] );
    }
    if ( ret == -2 )
    {
        giet_tty_printf("\n  error : %s cannot be killed\n", argv[0] );
    }
}

///////////////////////////////////////////////
static void cmd_ps(int argc, char** argv)
{
    giet_tasks_status();
}

////////////////////////////////////////////////////////////////////
struct command_t cmd[] =
{
    { "help",       cmd_help },
    { "proctime",   cmd_proctime },
    { "ls",         cmd_ls },
    { "mkdir",      cmd_mkdir },
    { "cp",         cmd_cp },
    { "rm",         cmd_rm },
    { "rmdir",      cmd_rmdir },
    { "mv",         cmd_mv },
    { "exec",       cmd_exec },
    { "kill",       cmd_kill },
    { "ps",         cmd_ps },
    { NULL,         NULL }
};

// shell

///////////////////////////////////////
static void parse(char *buf, int count)
{
    int argc = 0;
    char* argv[MAX_ARGS];
    int i;
    int len = strlen(buf);

    // build argc/argv
    for (i = 0; i < len; i++)
    {
        if (buf[i] == ' ')
        {
            buf[i] = '\0';
        }
        else if (i == 0 || buf[i - 1] == '\0')
        {
            if (argc < MAX_ARGS)
            {
                argv[argc] = &buf[i];
                argc++;
            }
        }
    }

    if (argc > 0)
    {
        int found = 0;

        // try to match typed command with built-ins
        for (i = 0; cmd[i].name; i++)
        {
            if (strcmp(argv[0], cmd[i].name) == 0)
            {
                // invoke
                cmd[i].fn(argc, argv);
                found = 1;
                break;
            }
        }

        if (!found)
        {
            giet_tty_printf("undefined command %s\n", argv[0]);
        }
    }
}

////////////////////
static void prompt()
{
    giet_tty_printf("# ");
}

//////////////////////////////////////////
__attribute__ ((constructor)) void main()
//////////////////////////////////////////
{
    char c;
    char buf[BUF_SIZE];
    int count = 0;

    // get a private TTY
    giet_tty_alloc( 0 );

    // display first prompt
    prompt();

    while (1)
    {
        giet_tty_getc(&c);

        switch (c)
        {
        case '\b':      // backspace
            if (count > 0)
            {
                giet_tty_printf("\b \b");
                count--;
            }
            break;
        case '\n':      // new line
            giet_tty_printf("\n");
            if (count > 0)
            {
                buf[count] = '\0';
                parse((char*)&buf, count);
            }
            prompt();
            count = 0;
            break;
        case '\t':      // tabulation
            // do nothing
            break;
        case '\03':     // ^C
            giet_tty_printf("^C\n");
            prompt();
            count = 0;
            break;
        default:        // regular character
            if (count < sizeof(buf) - 1)
            {
                giet_tty_printf("%c", c);
                buf[count] = c;
                count++;
            }
        }
    }
} // end main()

// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

