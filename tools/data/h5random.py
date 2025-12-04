import h5py
import numpy as np
import argparse
import sys

def parse_arguments():
    parser = argparse.ArgumentParser(description='Create or compare datasets in an HDF5 file.')
    parser.add_argument('datasets', nargs='+', help='List of datasets in the format name:dtype:dim1,dim2,...')
    parser.add_argument('--seed', type=int, default=42, help='Random seed for reproducibility')
    parser.add_argument('--mode', type=str, choices=['create', 'compare'], required=True, help='Mode of operation: create or compare')
    parser.add_argument('--file', type=str, help='HDF5 file for create mode or input file for compare mode')
    return parser.parse_args()

def rng_array(dtype_str, shape):
    dtype = np.dtype(dtype_str)
    if np.issubdtype(dtype, np.integer):
        info = np.iinfo(dtype)
        return np.random.randint(0, info.max, size=shape, dtype=dtype)
    elif np.issubdtype(dtype, np.floating):
        return np.random.uniform(-1, 1, size=shape).astype(dtype)
    else:
        raise ValueError(f"Unsupported data type: {dtype_str}")
    return np.random.rand(*dims).astype(dtype)
  

def create_hdf5_file(datasets, seed, filename):
    np.random.seed(seed)
    datasets.sort()
    with h5py.File(filename, 'w') as hdf_file:
        for dataset_spec in datasets:
            name, dtype, dims = dataset_spec.split(':')
            dims = list(map(int, dims.split(',')))
            hdf_file.create_dataset(name, data=rng_array(dtype, dims))
    print(f"HDF5 file '{filename}' created successfully with datasets {datasets}.")

def compare_datasets(datasets, seed, input_file):
    np.random.seed(seed)
    datasets.sort()
    with h5py.File(input_file, 'r') as hdf_file:
        for dataset_spec in datasets:
            name, dtype, dims = dataset_spec.split(':')
            dims = list(map(int, dims.split(',')))
            generated_data = rng_array(dtype, dims)
            stored_data = hdf_file[name][:]
            if stored_data.shape == generated_data.shape and np.array_equal(generated_data, stored_data):
                print(f"Dataset '{name}' matches.")
                sys.exit(0)
            else:
                print(f"Dataset '{name}' does not match.")
                sys.exit(1)

def main():
    args = parse_arguments()
    if args.mode == 'create':
        if not args.file:
            print("Error: --file must be specified in create mode.")
            return
        create_hdf5_file(args.datasets, args.seed, args.file)
    elif args.mode == 'compare':
        if not args.file:
            print("Error: --file must be specified in compare mode.")
            return
        compare_datasets(args.datasets, args.seed, args.file)

if __name__ == '__main__':
    main()
