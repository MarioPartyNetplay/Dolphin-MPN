#!/usr/bin/env python3
"""
Script to automatically fix minizip-ng headers by adding missing includes.
This script adds the mz_strm.h include to mz_zip_rw.h to fix callback type definitions.
"""

import os
import re
import sys

def fix_mz_zip_rw_header(file_path):
    """Add missing mz_strm.h include to mz_zip_rw.h"""
    
    if not os.path.exists(file_path):
        print(f"Error: File {file_path} not found")
        return False
    
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Check if mz_strm.h is already included
    if '#include "mz_strm.h"' in content or '#include <mz_strm.h>' in content:
        print(f"mz_strm.h already included in {file_path}")
        return True
    
    # Find the right place to insert the include (after the copyright header)
    lines = content.split('\n')
    
    # Look for the first #ifndef or #include after the copyright block
    insert_index = None
    for i, line in enumerate(lines):
        if line.strip().startswith('#ifndef') or line.strip().startswith('#include'):
            insert_index = i
            break
    
    if insert_index is None:
        print(f"Could not find insertion point in {file_path}")
        return False
    
    # Insert the include
    lines.insert(insert_index, '#include "mz_strm.h"')
    
    # Write back the file
    with open(file_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))
    
    print(f"Successfully added mz_strm.h include to {file_path}")
    return True

def fix_mz_zip_header(file_path):
    """Add missing mz_strm.h include to mz_zip.h if needed"""
    
    if not os.path.exists(file_path):
        print(f"Error: File {file_path} not found")
        return False
    
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Check if mz_strm.h is already included
    if '#include "mz_strm.h"' in content or '#include <mz_strm.h>' in content:
        print(f"mz_strm.h already included in {file_path}")
        return True
    
    # Check if this file uses callback types
    if 'mz_stream_read_cb' in content or 'mz_stream_write_cb' in content:
        lines = content.split('\n')
        
        # Look for the first #ifndef or #include after the copyright block
        insert_index = None
        for i, line in enumerate(lines):
            if line.strip().startswith('#ifndef') or line.strip().startswith('#include'):
                insert_index = i
                break
        
        if insert_index is not None:
            # Insert the include
            lines.insert(insert_index, '#include "mz_strm.h"')
            
            # Write back the file
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write('\n'.join(lines))
            
            print(f"Successfully added mz_strm.h include to {file_path}")
            return True
    
    print(f"No callback types found in {file_path}, skipping")
    return True

def fix_mz_strm_os_posix(file_path):
    """Wrap O_NOFOLLOW usage with #ifdef O_NOFOLLOW ... #endif in mz_strm_os_posix.c"""
    if not os.path.exists(file_path):
        print(f"Error: File {file_path} not found")
        return False

    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # Patch the line: mode_open |= O_NOFOLLOW;
    # Only patch if not already inside an #ifdef
    if 'mode_open |= O_NOFOLLOW;' in content and '#ifdef O_NOFOLLOW' not in content:
        new_content = re.sub(
            r'(\s+if \(mode & MZ_OPEN_MODE_NOFOLLOW\)\s*)mode_open \|= O_NOFOLLOW;',
            r'\1#ifdef O_NOFOLLOW\n    mode_open |= O_NOFOLLOW;\n#endif',
            content
        )
        if new_content == content:
            # fallback: patch any occurrence
            new_content = content.replace(
                'mode_open |= O_NOFOLLOW;',
                '#ifdef O_NOFOLLOW\n        mode_open |= O_NOFOLLOW;\n#endif'
            )
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(new_content)
        print(f"Patched O_NOFOLLOW usage in {file_path}")
        return True
    else:
        print(f"No O_NOFOLLOW patch needed in {file_path}")
        return True

def main():
    """Main function to fix all minizip headers and patch O_NOFOLLOW usage"""
    
    # Get the minizip-ng directory path (relative to this script)
    minizip_dir = "minizip-ng"
    
    if not os.path.exists(minizip_dir):
        print(f"Error: minizip-ng directory not found at {minizip_dir}")
        print("Please run this script from the Externals/minizip-ng directory")
        return 1
    
    print("Fixing minizip-ng headers...")
    
    # Fix mz_zip_rw.h
    mz_zip_rw_path = os.path.join(minizip_dir, "mz_zip_rw.h")
    if not fix_mz_zip_rw_header(mz_zip_rw_path):
        return 1
    
    # Fix mz_zip.h if needed
    mz_zip_path = os.path.join(minizip_dir, "mz_zip.h")
    if not fix_mz_zip_header(mz_zip_path):
        return 1
    
    # Patch mz_strm_os_posix.c for O_NOFOLLOW
    mz_strm_os_posix_path = os.path.join(minizip_dir, "mz_strm_os_posix.c")
    if not fix_mz_strm_os_posix(mz_strm_os_posix_path):
        return 1
    
    print("Header fixes completed successfully!")    
    return 0

if __name__ == "__main__":
    sys.exit(main()) 