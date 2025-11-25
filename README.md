# TA Grading Simulation

## Description
This program simulates multiple TAs marking student exams concurrently using shared memory and semaphores.  
Each TA can:
- Mark questions on a student’s exam.
- Modify the grading rubric.
- Load the next exam once all questions are marked.  

The simulation prints which TA marks which question and which TA modifies the rubric, showing the flow of concurrent grading.

---

## Files
- `partB_<student1>_<101314298>.cpp` – main program
- `partA_<student1>_<101314298>.cpp` – (reference code, no concurrency)
- `rubric.txt` – rubric file (5 lines)  
- `exams/` – folder containing exam files named `exam_0001.txt`, `exam_0002.txt`, etc. Each exam file should start with the student number on the first line, followed by the exam content.

---

## Compilation
```g++ -o ta_sim partB.cpp -lpthread```

---

## Running the program

Run the program with the desired number of TAs (minimum 2)

```./ta_sim <num_TAs>```

For example, to simulate 4 TAs:

```./ta_sim 4```

---

## Output

The output shows three types of events:

1. Which TA modifies which rubric line:
   
```TA 2 modified rubric line 1```


3. Which TA marks which question for which student:
   
```TA 3 marked Question 2 for Student 1```


5. Which TA loads the next exam:
   
```TA 1 loaded next exam (student 2)```


All TAs mark question concurrently, but each question is only marked once per student. The shared memory and semaphores ensure safe concurrent access.

---

## Test Cases

To test the program:

  1. Place all exam files in the ```exams/``` folder
  2. Ensure ```rubric.txt``` exists in the same directory as the program
  3. Run the program with different numbers of TAs (e.g., 2, 3, 4) to observe concurrency behaviour

---

## Design Discussion

Our TA grading simulation uses shared memory to allow multiple TA processes to access the same exam and rubric data. The program is designed to satisfy the classical three requirements of the critical section problem:

1. **Mutual Exclusion**  
   Access to shared data such as `exam_questions_marked`, `exam_loaded`, `student_number`, and the `exam` text is protected using a semaphore (`mutex`). Only one TA process can modify these critical pieces of data at a time, preventing race conditions.

2. **Progress**  
   If no TA is currently modifying the shared data, and a TA wants to mark a question or load the next exam, it is guaranteed to proceed after acquiring the semaphore. The semaphore ensures that waiting processes will eventually enter the critical section in the order they attempt to access it.

3. **Bounded Waiting**  
   Each TA can mark only one question per iteration, and the shared semaphore ensures that every TA gets a chance to enter the critical section. The random sleep intervals and repeated checking of unmarked questions make starvation unlikely. Therefore, each TA will eventually mark all questions for every exam.

The design uses **shared memory** to store the exam data and a **mutex semaphore** to control access. A **print_lock semaphore** ensures proper console output without overlapping access from multiple TAs.

