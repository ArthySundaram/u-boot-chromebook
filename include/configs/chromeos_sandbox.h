/*
 * Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __configs_chromeos_sandbox_h__
#define __configs_chromeos_sandbox_h__

#include <configs/sandbox.h>

#define CONFIG_CHROMEOS

/* Device Tree */
#undef CONFIG_DEFAULT_DEVICE_TREE
#define CONFIG_DEFAULT_DEVICE_TREE chromeos_sandbox

#endif /* __configs_chromeos_sandbox_h__ */
