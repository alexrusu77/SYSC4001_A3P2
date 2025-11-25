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
- `grading_sim_<student1>_<101314298>.cpp` – main program  
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


