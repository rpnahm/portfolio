# Job Scheduling Project
The purpose of this project is to build a job scheduling project with three different scheduling algorithms. 

## Explanation
- The "jobs" for this project are text to speech conversions using piper which found at :https://github.com/rhasspy/piper and downloaded as follows:

``` 
curl -L https://github.com/rhasspy/piper/releases/download/v1.2.0/piper_amd64.tar.gz > piper.tar.gz
tar xvzf piper.tar.gz
```

Then make the executable by running make, and execute by runnign 
```
./jobsched <test.txt
``` 
with any of the test.txt files. 

This can also be run manually with the following instructions:
```
The submit command defines a new text-to-speech job, and names the input text file to convert. submit should not perform the conversion itself! Instead, submit should add the job to the queue, and display a unique integer job ID generated internally by your program. (Just start at one and count up.) The job will then run in the background when selected by the scheduler. When done, it should produce an output file called jobN.wav, regardless of the name of the input file. So, Job 1 will produce job1.wav, Job 2 will produce job2.wav, etc.

The nthreads command should start n background threads that perform text-to-speech tasks on the submitted jobs. The nthreads command may only be given once in any session. If it is given a second time, it should fail.

The list command lists all of the jobs currently known, giving the job id, current state (WAITING, RUNNING, or DONE), input filename, size of the input file, and size of the output file (if DONE). It should also display the total size of all input files, the total size of all output files (for DONE jobs), the average turnaround time (of DONE jobs), and average response time (of DONE jobs.) You can format this output in any way that is consistent and easy to read.

The wait command takes a jobid and pauses until that job is done running. Once complete, it should display the final status of the job (success or failure) and the time at which it was submitted, started running, and completed. (If the job was already complete, then it should just display the relevant information immediately.)

The waitall command should block until all jobs in the queue are in the DONE state.

The delete command takes a jobid and then removes the job from the queue, along with its output file. However, a job cannot be deleted while it is in the RUNNING state. In this case, display a suitable error and refuse to delete the job.

The schedule command should select the scheduling algorithms used: fcfs is first-come-first-served, and sjf is shortest-job-first, and balanced should prefer the shortest job, but make some accomodation to ensure that no job is starved indefinitely.

The quit command should immediately exit the program, regardless of any jobs in the queue. (If end-of-file is detected on the input, the program should quit in the same way.)

The help command should display the available commands in a helpful manner.
```

have fun! 