# Context
## State Machines Workshop

This workshop involves designing an embedded system to control the state of an LED via hardware and software. You will be provided a circuit that includes three buttons:
- **Red**: When this button is pushed, the LED turns off.
- **Yellow**: When this button is pushed, the LED starts blinking every second.
- **Green**: When this button is pushed, the LED turns on.

When a button is pressed, it will connect a GPIO pin with the 3.3 V bus (provided by RPI), sending a signal to that GPIO pin. You will take this circuit, translate it into a state machine, model this state machine in FPP, then implement in C++. 

**Circuit & RPI 0 2 W Pinout:**
![alt text](<Pasted image 20261019014826.png>)
![alt text](<Pasted image 20261109190744.png>)

This should be both a fun workshop as well as good experience. Going through this process, you will familiarize yourself with Git, GitHub, reading technical documentation, writing technical documentation, implementing software based on a design, and teamwork in engineering. Using GitHub allows you to publicly display this project (for future employers & other engineers), writing documentation teaches you just how important documentation is in engineering, and learning to read documentation will be useful no matter which industry you end up in.

## Objectives

**1.** Learn to develop a state machine diagram for a physical system
**2.** Learn how to model state machines in FPP
**3.** Learn how to implement state machine logic
**4.** Learn how to configure Raspberry Pi to receive hardware signals over GPIO

## Roles

Each person in the group will have specific responsibilities. Each of these roles are crucial in engineering, and learning to work in these roles will provide dividends for the rest of your career. 

**Documentation Writer**:
- Write documentation for the project along the way. 
- Document which components were used, why design decisions were made, etc.

**Documentation Reader**:
- Read F' documentation.
- Search for solutions to syntax errors, configuration, coding rules, etc.

**Implementor**:
- Develop FPP/HPP/CPP
- Manage Git repository

**Try to organize yourselves such that you are in an uncomfortable position.** For example, if you have done a lot of software development, but haven't done documentation, choose the documentation writer. If you haven't done a lot of coding, choose the implementor. If you haven't done any technical projects before, choose the documentation reader. Also, try to get at least one person on your team who has already done the LED blinker tutorial. 

# Workshop Phases

Throughout each of these phases:

**Documentation Writers**: 
- Make sure everything is getting documented. Include design decisions, bugs you ran into, which references you used and which references drove your design. 

**Implementors**: 
- Try to practice good Git usage by making frequent commits (like at each phase boundary).

**Documentation Readers**: 
- Work with both the implentor and documentation writer by providing accurate information
- Make sure the documentation writer knows where certain information was found

## **0. Setup a GitHub Repository**:

**Objectives**:
- Create an F' project
- Push this project to a public GitHub repository
- Get each team member added as a contributor to this repository

## **1. Create a State-Machine Diagram**

**Objectives**:
- You should define a state machine that includes every state listed in the context above. 
- You should also clearly define the state transitions between each state. 

Here is an example state machine diagram for a `LaserManager` component which manages the state of a laser device, I recommend using a software like [LucidChart](https://lucid.app/) to create a similar diagram.

![alt text](<Pasted image 20261019020728.png>)

## **2. Design a LED Manager Component**

**Objectives**
- Design an LED Manager component
	- Define all of the necessary commands, telemetry, events, etc.
	- What rate group should it be attached to?
- Translate your state machine generated in **(1)** into the FPP modeling language

## **3. Implement State Machine Logic**

**Objectives**:
- Figure out what behavior should happen in each state
- When events get logged
- When telemetry values update

## **4. Integrate into a Topology**:

**Objectives**:
- Create a deployment that will run on the RPI
- Wire your custom component with the necessary connections

## **5. Deploy and Test on Hardware**

**Objectives**:
- Cross compile your deployment onto the RPI
- Test your code

# **Resources**

**F' Specific**:
- [F' Cheatsheet](https://fprime.jpl.nasa.gov/cheatsheet.pdf)
- [Creating a new F' project](https://fprime.jpl.nasa.gov/latest/docs/getting-started/installing-fprime/#creating-a-new-f-project)
- [F' Cross-Compilation Setup](https://fprime.jpl.nasa.gov/latest/docs/tutorials/cross-compilation/#f-running-on-arm-linux-tutorial)
- [Configuring GPIO pin in F'](https://fprime.jpl.nasa.gov/latest/tutorials-led-blinker/docs/led-blinker/#7-led-blinker-full-system-integration)
- [Components - User Guide](https://nasa.github.io/fpp/fpp-users-guide.html#Defining-State-Machines)
- [State Machines - How To](https://fprime.jpl.nasa.gov/latest/docs/how-to/develop/define-state-machines/#when-to-use-state-machines)
- [State Machines - User Guide](https://nasa.github.io/fpp/fpp-users-guide.html#Defining-State-Machines)
- [LinuxGpioDriver - User Guide](https://fprime.jpl.nasa.gov/latest/Drv/LinuxGpioDriver/docs/sdd/#4-usage)
