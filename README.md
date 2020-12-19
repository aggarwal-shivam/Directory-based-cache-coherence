# README
This repository implements a Directory based cache coherence protocol. This was implemented as an assignment in the course Advancced Computer Architecture at IIT Kanpur in the Autumn semester 2020. 

## Contributors
1. Shivam Aggarwal (shivama20@iitk.ac.in)
2. Boppanna Tej Kiran (tejkiranb20@iitk.ac.in)

We are given four parallel multi-threaded programs, which can be found [here](https://drive.google.com/drive/folders/1JtJAXcDaLAOqOojsKYNUW86suBRjyv3m?usp=sharing). First we need to generate their trace files using the PIN tool. Then we need to break this thread into multiple threads based on thread IDs. For this purpose file named 'thread_breaker.cpp' is present. Cache is implemented in file named 'cache.h' and simulator is present in the file named 'simulator.cpp'

Problem description can be found [here](https://drive.google.com/file/d/1ljJ3WWWh7ulob-G2qdLPlKNYNF9laShW/view?usp=sharing).

Running instruction is present in the file named 'instructions.pdf'.

Report is present with name 'report.pdf'

