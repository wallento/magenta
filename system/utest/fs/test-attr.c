// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/time.h>

#include <magenta/syscalls.h>
#include <mxio/vfs.h>

#include "filesystems.h"

#define ROUND_DOWN(t, granularity) ((t) - ((t) % (granularity)))

int64_t nstimespec(struct timespec ts) {
    // assumes very small number of seconds in deltas
    return ts.tv_sec * MX_SEC(1) + ts.tv_nsec;
}

bool test_attr(void) {
    BEGIN_TEST;
    int64_t now = mx_time_get(MX_CLOCK_UTC);
    ASSERT_NEQ(now, 0, "mx_time_get only returns zero on error");

    int fd1 = open("::file.txt", O_CREAT | O_RDWR, 0644);
    ASSERT_GT(fd1, 0, "");

    struct timespec ts[2];
    ts[0].tv_nsec = UTIME_OMIT;
    ts[1].tv_sec = (long)(now / MX_SEC(1));
    ts[1].tv_nsec = (long)(now % MX_SEC(1));

    // make sure we get back "now" from stat()
    ASSERT_EQ(futimens(fd1, ts), 0, "");
    struct stat statb1;
    ASSERT_EQ(fstat(fd1, &statb1), 0, "");
    now = ROUND_DOWN(now, test_info->nsec_granularity);
    ASSERT_EQ(statb1.st_mtim.tv_sec, (long)(now / MX_SEC(1)), "");
    ASSERT_EQ(statb1.st_mtim.tv_nsec, (long)(now % MX_SEC(1)), "");
    ASSERT_EQ(close(fd1), 0, "");

    mx_nanosleep(mx_deadline_after(test_info->nsec_granularity));

    ASSERT_EQ(utimes("::file.txt", NULL), 0, "");
    struct stat statb2;
    ASSERT_EQ(stat("::file.txt", &statb2), 0, "");
    ASSERT_GT(nstimespec(statb2.st_mtim), nstimespec(statb1.st_mtim), "");

    ASSERT_EQ(unlink("::file.txt"), 0, "");

    END_TEST;
}

bool test_blksize(void) {
    BEGIN_TEST;

    int fd = open("::file.txt", O_CREAT | O_RDWR, 0644);
    ASSERT_GT(fd, 0, "");

    struct stat buf;
    ASSERT_EQ(fstat(fd, &buf), 0, "");
    ASSERT_GT(buf.st_blksize, 0, "blksize should be greater than zero");
    ASSERT_EQ(buf.st_blksize % VNATTR_BLKSIZE, 0, "blksize should be a multiple of VNATTR_BLKSIZE");
    ASSERT_EQ(buf.st_blocks, 0, "Number of allocated blocks should be zero");

    char data = {'a'};
    ASSERT_EQ(write(fd, &data, 1), 1, "Couldn't write a single byte to file");
    ASSERT_EQ(fstat(fd, &buf), 0, "");
    ASSERT_GT(buf.st_blksize, 0, "blksize should be greater than zero");
    ASSERT_EQ(buf.st_blksize % VNATTR_BLKSIZE, 0, "blksize should be a multiple of VNATTR_BLKSIZE");
    ASSERT_GT(buf.st_blocks, 0, "Number of allocated blocks should greater than zero");
    ASSERT_EQ(close(fd), 0, "");

    blkcnt_t nblocks = buf.st_blocks;
    ASSERT_EQ(stat("::file.txt", &buf), 0, "");
    ASSERT_EQ(buf.st_blocks, nblocks, "Block count changed when closing file");

    ASSERT_EQ(unlink("::file.txt"), 0, "");

    END_TEST;
}

RUN_FOR_ALL_FILESYSTEMS(attr_tests,
    RUN_TEST_MEDIUM(test_attr)
    RUN_TEST_MEDIUM(test_blksize)
)
