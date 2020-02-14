
enum signalStates {INERT, GO, RESOLVE};
byte signalState = INERT;
Timer goBroadcast; // Used to control how long you broadcast the go signal, hopefully this helps master pick up signals while it's polling both faces
bool runGoBroadcastTimer = false; // true if still waiting for delay to finish

Timer masterColorSwitchTimer;
int masterColorSwitchLengthDefault = 2000;
int masterColorSwitchDelta = 0;
int masterColorSwitchLength = masterColorSwitchLengthDefault; // Slowly decrease

#define OFF_DURATION 100

Timer winnerDisplayLengthTimer;
const int winnerDisplayLength = 3000;
bool clearedWinner = false;
byte spoonsMode = 0; //0 = none; 1 = winners; 2 = losers;

int masterColorIndex = 0;
int masterValue = 99;

byte adjacentMasterFace = 6;

enum playerRankValues {RANK_NONE, RANK_LOSE, RANK_MID, RANK_WIN};
const Color playerColors[] = {WHITE, RED, YELLOW, GREEN};
const Color masterColors[] = { RED, GREEN, BLUE , YELLOW, WHITE};
const int masterColorNum = sizeof(masterColors) / sizeof(masterColors[0]);
const int masterValues[] = {1, 3, 6};
const int masterValuesNum = sizeof(masterValues) / sizeof(masterValues[0]);

bool isMaster = false;

// Stores most recent pattern elements. Most recent is on the left.
int lastElements[3][2] = { {99,99}, // Color index, number
                           {99,99},
                           {99,99} };

byte playerRanks[] = {6,6,6,6,6,6};
byte numPlayers = 0;
byte currentRank = 0;

byte currentPlayerColor = 0;
                    
const int lastElementsNum = sizeof(lastElements) / sizeof(lastElements[0]);

void setup() {
  randomize();
}

void masterInit() {
    signalState = RESOLVE; // Don't know if this is the best place for this TODO

    setColor(RED); // Change the color instantly so that the user knows the master was created. TODO Maybe should flash or rotate or something.
}

void loop() {
    byte sendData = (signalState << 1) + (currentPlayerColor << 3);
    if (isMaster) {
        sendData = sendData + 1; //tell adjacent tiles they are next to the master
        masterLoop();
    } else {
      nonMasterLoop();
    }
    displaySignalState();
  
    setValueSentOnAllFaces(sendData);
    if (isMaster) setSendValuesByRanking();
  
    if (buttonLongPressed()) {
        isMaster = !isMaster;
        if (isMaster) {
            masterInit();
        }
    }
}

bool isValidPattern() {
    // Doubles
    if ((lastElements[0][0] == lastElements[1][0]) && (lastElements[0][0] != 99)) { // Check color double
        return true;
    }
    if ((lastElements[0][1] == lastElements[1][1]) && (lastElements[0][1] != 99)) { // Check value double
        return true;
    }

    return false;
}

void setMasterResult(int inputFace, bool isCorrect) {
    Color inputColor = GREEN; // TODO Maybe CYAN
    if (!isCorrect) {
        inputColor = RED;
    }
    // Color on side input was received from
    setColorOnFace(inputColor, (inputFace + 1) % 6);
    setColorOnFace(inputColor, inputFace);
    setColorOnFace(inputColor, (inputFace + 5) % 6);

    // Color on opposite side
    setColorOnFace(OFF, (inputFace + 2) % 6);
    setColorOnFace(OFF, (inputFace + 3) % 6);
    setColorOnFace(OFF, (inputFace + 4) % 6);
}

// Sets the stored pattern to 99 (init val)
void resetStoredPattern() {
    for (int i=0; i<lastElementsNum; i++) {
        lastElements[i][0] = 99;
        lastElements[i][1] = 99;
    }
}

void displayCombo(Color color, int value) {
    if (value >= 1) {
        setColorOnFace(color, 0);
    }
    if (value >= 3) {
        setColorOnFace(color, 2);
    }
    if (value >= 6) {
        setColorOnFace(color, 4);
    }
}

void masterLoop() {
  if (winnerDisplayLengthTimer.isExpired()) { // This timer controls how long we display the winning state, prevents reading new inputs
    spoonsMode = 0;
    if (!clearedWinner) {
        setColor(OFF);
        clearedWinner = true;
    }
    
    if (masterColorSwitchTimer.isExpired()) {  // SET NEXT MASTER COLOR
        masterColorIndex = random(masterColorNum - 1);
        masterValue = masterValues[random(masterValuesNum - 1)];
        // Shift all stored combos in lastElements to the right. TODO put this in a function ?
        for(int i=lastElementsNum-1; i>0; i--)
        {
            lastElements[i][0] = lastElements[i-1][0];
            lastElements[i][1] = lastElements[i-1][1];
        }
        lastElements[0][0] = masterColorIndex; // Overwrite first element with newest one
        lastElements[0][1] = masterValue;

        displayCombo(masterColors[masterColorIndex], masterValue);
        masterColorSwitchTimer.set(masterColorSwitchLength);

        masterColorSwitchLength = masterColorSwitchLength - masterColorSwitchDelta; // Slowly decrease
        
    } else if (masterColorSwitchTimer.getRemaining() < OFF_DURATION) { // Blink off for a bit
        setColor(OFF);
    } 

    FOREACH_FACE(f) {
      if (!isValueReceivedOnFaceExpired(f)
          && getSignalState(getLastValueReceivedOnFace(f)) == GO
          && getColorState(getLastValueReceivedOnFace(f)) == RANK_NONE) { // Received first player input
        // Check if valid hit
        bool isPlayerWin = isValidPattern();

        // initialize ranking system
        currentRank = 1;
        numPlayers = 0;
        for (int i = 0; i < 6; i++) {
          playerRanks[i] = 6;
          if (!isValueReceivedOnFaceExpired(i)) numPlayers++;
          if (i == f) {
            playerRanks[i] = 0;
          }
        }
        spoonsMode = 2;
        if (isPlayerWin) {
          spoonsMode = 1;
        }

        // Set winning side to GREEN and losing side to OFF. If wrong set first side to RED.
        setMasterResult(f, isPlayerWin);

        masterColorSwitchLength = masterColorSwitchLengthDefault; // reset switch length
        
        // reset pattern memory
        resetStoredPattern();

        winnerDisplayLengthTimer.set(winnerDisplayLength); // TODO This technically isn't winner display timer because it also displays mistakes
        clearedWinner = false;
        break;
      }
    }
  } //winner logic
  else if (currentRank < numPlayers) {
    if (spoonsMode == 1) {
      winnerDisplayLengthTimer.set(winnerDisplayLength); // TODO This technically isn't winner display timer because it also displays mistakes
    }
    FOREACH_FACE(f) {
      if (!isValueReceivedOnFaceExpired(f) && getSignalState(getLastValueReceivedOnFace(f)) == GO) { // Received next player input
        if (playerRanks[f] == 6) {
          playerRanks[f] = currentRank++;
        }
      }
    }
  }
}

void setSendValuesByRanking() {
  if (spoonsMode == 1) {
    FOREACH_FACE(f) {
      byte sendVal = (GO << 1) + 1;
      if(!isValueReceivedOnFaceExpired(f) && getSignalState(getLastValueReceivedOnFace(f)) != RESOLVE) {
        if (playerRanks[f] == 0) {
          sendVal = sendVal + (RANK_WIN << 3);
          setValueSentOnFace(sendVal, f);
        }
        else if (playerRanks[f] < numPlayers - 1) {
          sendVal = sendVal + (RANK_MID << 3);
          setValueSentOnFace(sendVal, f);
        }
        else if (playerRanks[f] == numPlayers - 1) {
          sendVal = sendVal + (RANK_LOSE << 3);
          setValueSentOnFace(sendVal, f);
        }
      }
      else {
        setValueSentOnFace((RESOLVE << 1) + 1, f); // forces player blinks to stay in resolve mode
      }
    }
  }

  if (spoonsMode == 2) {
    FOREACH_FACE(f) {
      if(!isValueReceivedOnFaceExpired(f) && getSignalState(getLastValueReceivedOnFace(f)) != RESOLVE) {
        byte sendVal = (GO << 1) + 1;
        if (playerRanks[f] < 6) {
          sendVal = sendVal + (RANK_LOSE << 3);
          setValueSentOnFace(sendVal, f);
        }
      }
      else {
        setValueSentOnFace((RESOLVE << 1) + 1, f); // forces player blinks to stay in resolve mode
      }
    }
  }
}

void nonMasterLoop() {
  FOREACH_FACE(f) {
    checkAdjacentMaster(f);
  }
  switch (signalState) {
    case INERT:
      inertLoop();
      break;
    case GO:
      if (runGoBroadcastTimer) { // Make sure timer runs only once
          goBroadcast.set(200);
      }
      if (!goBroadcast.isExpired()) {
          // Keep broadcasing GO for a time
          runGoBroadcastTimer = false;
      } else {
          goLoop();
      }
      break;
    case RESOLVE:
      resolveLoop();
      break;
  }
}

void inertLoop() {
  //set myself to GO
//  if (buttonSingleClicked()) {
  currentPlayerColor = 0;
  if (buttonDown()) {
    signalState = GO;
    runGoBroadcastTimer = true;
  }

  //listen for neighbors in GO
  FOREACH_FACE(f) {
    if (shouldConsiderFace(f) && !isValueReceivedOnFaceExpired(f)) {//a neighbor!
      if (getSignalState(getLastValueReceivedOnFace(f)) == GO) {//a neighbor saying GO!
        comparePlayerColor(getColorState(getLastValueReceivedOnFace(f)));
        signalState = GO;
        runGoBroadcastTimer = true;
      }
    }
  }
}

// make color commands propagate regardless of state, reset when signal state is INERT
void comparePlayerColor(byte c) {
  if (c > currentPlayerColor) {
    currentPlayerColor = c;
  }
}

void goLoop() {
    signalState = RESOLVE; //I default to this at the start of the loop. Only if I see a problem does this not happen
    
    //look for neighbors who have not heard the GO news
    FOREACH_FACE(f) {
        if (shouldConsiderFace(f) && !isValueReceivedOnFaceExpired(f)) {//a neighbor!
            comparePlayerColor(getColorState(getLastValueReceivedOnFace(f)));
            if (getSignalState(getLastValueReceivedOnFace(f)) == INERT) {//This neighbor doesn't know it's GO time. Stay in GO
              signalState = GO;
            }
        }
    }
}

void resolveLoop() {
  signalState = INERT;//I default to this at the start of the loop. Only if I see a problem does this not happen

  //look for neighbors who have not moved to RESOLVE
  FOREACH_FACE(f) {
    if (shouldConsiderFace(f) && !isValueReceivedOnFaceExpired(f)) {//a neighbor!
      comparePlayerColor(getColorState(getLastValueReceivedOnFace(f)));
      if (getSignalState(getLastValueReceivedOnFace(f)) == GO) {//This neighbor isn't in RESOLVE. Stay in RESOLVE
        signalState = RESOLVE;
      }
    }
  }
}

bool shouldConsiderFace(byte f) { //if adjacent to master, should I prop signals to this face?
  if (adjacentMasterFace == 6) return true;

  if (f == adjacentMasterFace
   || f == (adjacentMasterFace + 2) % 6
   || f == (adjacentMasterFace + 3) % 6
   || f == (adjacentMasterFace + 4) % 6) {
    return true;
  }
  return false;
}

void checkAdjacentMaster(byte f) {
  // if connected and                          is adjacent master
  if (!isValueReceivedOnFaceExpired(f) && (getLastValueReceivedOnFace(f) & 1) == 1) {
    adjacentMasterFace = f;
  }
  else if (adjacentMasterFace == f) { //reset if signal is not from master when it is supposed to be
    adjacentMasterFace = 6;
  }
}

void displaySignalState() {
  switch (signalState) {
    case INERT:
      setColor(OFF);
      break;
    case GO:
      setColor(playerColors[currentPlayerColor]);
    case RESOLVE:
      if (!isMaster) {
        setColor(playerColors[currentPlayerColor]);
      }
      break;
  }
}

byte getColorState(byte data) {
  return ((data >> 3) & 3);//returns bits C and D
}

byte getSignalState(byte data) {
  return ((data >> 1) & 3);//returns bits C and D
}
