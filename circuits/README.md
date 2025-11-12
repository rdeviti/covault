# MPC Circuit Files

This folder contains files describing pre-compiled Multi-Party Computation (MPC) circuits for EMP Toolkit.

## Source

The circuit files in this directory originate from two primary sources:

* **External Library:** Some circuit files have been directly taken or adapted from [EMP-toolkit](https://github.com/emp-toolkit/emp-tool) library, specifically from the [emp-tool/circuits](https://github.com/emp-toolkit/emp-tool/tree/master/emp-tool/circuits) directory. These circuits provide efficient implementations of fundamental arithmetic and logical operations.

* **Project-Specific Generation:** Other circuit files present in this folder have been generated specifically for the needs of this project. These are typically created by corresponding C++ files within the project. These C++ programs take high-level descriptions of computations and compile them down to the low-level circuit representations stored in these files.

## Format

The format of these circuit files is specific to the MPC framework being used. They typically describe the individual gates within the circuit, their types (e.g., addition, multiplication, comparison), and the connections between them.

## Note: Do Not Modify Manually

Any changes to the underlying computations should be made at the higher level, either by selecting different pre-existing circuits or by modifying the C++ code that generates the project-specific circuits and then re-running the generation process.

## License

The circuit files in this directory are licensed under the **MIT License**. This applies both to circuits adapted from the [EMP-toolkit](https://github.com/emp-toolkit/emp-tool) (with their respective copyright holders) and to circuits generated specifically for this project (with our project's copyright holders).