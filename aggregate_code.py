import os
import argparse

def aggregate_code(root_path, output_filename="combined_code.txt"):
    """
    Recursively finds all .cpp and .h files in a directory and aggregates
    them into a single, well-formatted text file.

    Args:
        root_path (str): The path to the root directory to scan.
        output_filename (str): The name of the file to save the combined code to.
    """
    # --- Step 1: Validate the input path ---
    if not os.path.isdir(root_path):
        print(f"Error: The path '{root_path}' does not exist or is not a directory.")
        return

    # --- Step 2: Define target files and gather them ---
    target_extensions = ('.cpp', '.h')
    source_files = []
    for dirpath, _, filenames in os.walk(root_path):
        for filename in filenames:
            if filename.endswith(target_extensions):
                # Store the full, relative path for sorting and display
                full_path = os.path.join(dirpath, filename)
                source_files.append(full_path)

    # Sort files for a consistent order (e.g., headers before sources)
    source_files.sort()
    
    if not source_files:
        print(f"No '.cpp' or '.h' files found in '{root_path}'.")
        return

    # --- Step 3: Write to the output file ---
    try:
        with open(output_filename, 'w', encoding='utf-8') as outfile:
            print(f"Found {len(source_files)} source files. Aggregating into '{output_filename}'...")
            
            for file_path in source_files:
                # Use a consistent path separator for the LLM
                display_path = file_path.replace('\\', '/')
                
                # Create a clear header for each file
                header = f"// =================================================================\n"
                header += f"// File: {display_path}\n"
                header += f"// =================================================================\n\n"
                
                outfile.write(header)
                
                try:
                    with open(file_path, 'r', encoding='utf-8', errors='ignore') as infile:
                        outfile.write(infile.read())
                        outfile.write("\n\n") # Add a couple of newlines for spacing
                except Exception as e:
                    outfile.write(f"// --- Error reading file: {e} ---\n\n")

        print("Aggregation complete!")
        print(f"Output file created: '{output_filename}'")

    except IOError as e:
        print(f"Error writing to file '{output_filename}': {e}")


if __name__ == "__main__":
    # --- Step 4: Setup command-line argument parsing ---
    parser = argparse.ArgumentParser(
        description="Aggregate C/C++ source (.cpp, .h) files from a directory into a single text file for LLM context.",
        formatter_class=argparse.RawTextHelpFormatter
    )
    
    parser.add_argument(
        "root_path",
        help="The root directory of your source code (e.g., 'src')."
    )
    
    parser.add_argument(
        "-o", "--output",
        default="combined_code.txt",
        help="The name of the output file (default: combined_code.txt)."
    )
    
    args = parser.parse_args()
    
    aggregate_code(args.root_path, args.output)