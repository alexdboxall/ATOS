#pragma once

/*
* errno.h - Error Numbers
*
*
* Error codes returned by various kernel functions.
*
*/

#ifndef NULL
#define NULL	((void*) 0)
#endif

#define ENOSYS			1			// Not implemented
#define ENOMEM			2			// Not enough memory
#define ENODEV			3			// No such device
#define EALREADY		4			// Device is already registered
#define ENOTSUP			5			// Operation not supported
#define EDOM			6			// Parameter outside of domain
#define EINVAL			7			// Invalid argument
#define EEXIST			8			// File already exists
#define ENOENT			9			// No such file or directory
#define EIO				10			// Input / output error
#define EACCES			11			// Permission denied
#define ENOSPC			12			// No space left on device
#define ENAMETOOLONG	13			// Filename too long
#define ENOTDIR			14			// Not a directory
#define EISDIR			15			// Is a directory
#define ELOOP			16			// Too many loops in symbolic link resolution
#define EROFS			17			// Read-only filesystem
#define EAGAIN			18			// Resource temporarily unavailable
#define EWOULDBLOCK		EGAIN		// Operation would block, but for historical reasons
									//			is often the same thing as EWOULDBLOCK