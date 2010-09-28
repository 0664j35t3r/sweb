// Projectname: SWEB
// Simple operating system for educational purposes
//
// Copyright (C) 2010  Daniel Gruss, Matthias Reischer
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.


#ifndef wait_h___
#define wait_h___

#define WEXITED 4

//pid typedefs
#ifndef PID_T_DEFINED
#define PID_T_DEFINED
typedef int pid_t;
#endif // PID_T_DEFINED

/**
 * posix method signature
 * do not change the signature!
 */
extern pid_t waitpid(pid_t pid, int *status, int options);



#endif // wait_h___


