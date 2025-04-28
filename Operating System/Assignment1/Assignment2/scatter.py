# scatter.py (Corrected)

def scatter_program(program_file, floppy_image, sector_map):
    """
    Scatters the program file across the floppy image based on the sector map.

    Args:
        program_file: Path to the compiled bubble sort program.
        floppy_image: Path to the floppy disk image file.
        sector_map: A dictionary mapping section names to sector numbers.
    """
    try:
        with open(program_file, "rb") as program, open(floppy_image, "r+b") as floppy:
            for sector_num, section_info in sector_map.items():
                section_name, offset = section_info  # Unpack section name and offset
                
                # Read a 512-byte chunk (assuming each section fits in a sector)
                program.seek(offset)  # Seek to the start of the section based on the offset
                chunk = program.read(512)

                if len(chunk) > 512:
                    raise ValueError(f"Section '{section_name}' is larger than 512 bytes")

                # Go to the appropriate sector
                floppy.seek(sector_num * 512)

                # Write the chunk
                floppy.write(chunk)

                print(f"Section '{section_name}' written to sector {sector_num}")

    except FileNotFoundError:
        print(f"Error: One or both files not found.")
    except Exception as e:
        print(f"An error occurred: {e}")

if __name__ == "__main__":
    program_file = "bubble_sort.bin"  # Output from NASM
    floppy_image = "floppy.img"

    # Define where each section should go (modify as needed)
    # sector number : (section name, offset in the binary)
    sector_map = {
        2: ("section1", 0),      # Sector 2: section1 starts at offset 0
        3: ("section2", 512),    # Sector 3: section2 starts at offset 512
        4: ("section3", 1024)    # Sector 4: section3 starts at offset 1024
    }

    scatter_program(program_file, floppy_image, sector_map)
