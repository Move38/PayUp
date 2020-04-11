// State machine map: https://docs.google.com/presentation/d/1CsAItlWN2fxssGJggbDrWCFktAbGhIYJ73E4aiDmlCU/edit#slide=id.g6f17dd6178_0_0

// **********************************************************************
// **** TUNABLE CONSTANTS ***********************************************
// **********************************************************************
const int masterColorSwitchGapLength = 100; // Dark time in between color switches
const int masterColorSwitchLengthMin = 600; // 600
const int masterColorSwitchLengthMax = 1200; // 1200
const int masterColorSwitchLengthDelta = 100;

const int phaseDurations[] = {40, 25, 5, 15, 8};
const int phaseDeltas[] = {-15, 0, 80, 0, -50};
const int phaseDurations_v0[] = {20, 8, 5, 5, 8};
const int phaseDeltas_v0[] = {-30, 0, 80, 0, -50};
const int numPhases = sizeof(phaseDeltas) / sizeof(phaseDeltas[0]);

const int lonePieceActivationMin = 1000;
const int lonePieceActivationMax = 8000;
const int masterResultDisplayLength = 4500;
const int loserResultDisplayLength = 2000;
const int winnerPendingWaitLength = 2000;
const int resetStateLength = 300;
const int masterSetupStateLength = 1500;
const int masterSetupDontSendGoLength = 250;
const int pipeDisplayLength = 500;
const int pipePropagationAnimationLength = 50;
const int inputDisplayLength = 1000;
const int randomAloneDeathChance = 5;
const int goDelay = 45;
const int PIPE_BRIGHTNESS = 64;

// Have direction and step size. Every master spin add step_size * direction. If master spin timer is greater
// or less than bounds then change direction.

// **********************************************************************
// **** GLOBAL VARIABLES ************************************************
// **********************************************************************

const Color playerRankColors[] = {WHITE, RED, YELLOW, GREEN, MAGENTA};
// const Color masterColors[] = { RED, GREEN, BLUE , YELLOW, WHITE};
const Color masterColors[] = {CYAN, MAGENTA, YELLOW};

byte masterColorIndex = 0;
byte masterValue = 99;
int masterColorSwitchLength = masterColorSwitchLengthMax;

// int masterColorSwitchLengthDirection = -1;
int phaseIndex = 0;
int phaseStepsTaken = 0;

byte adjacentMasterFace = 6;

enum playerRankValues {RANK_NONE, RANK_LOSE, RANK_MID, RANK_WIN, RANK_RESET};
const byte masterColorNum = 3;
const byte masterValues[] = {6};
const byte masterValuesNum = 1;

// Stores most recent pattern elements. Most recent is on the left.
byte lastElements[3][2] = {{99,99}, // Color index, number
                           {99,99},
                           {99,99}};

byte playerRanks[] = {6,6,6,6,6,6};
byte numPlayersAtInputTime = 0;
byte masterNextRankToAssign = 0;

byte currentPlayerRankSignal = 0;
byte currentPlayerRankCache = 0;
                    
const int lastElementsNum = sizeof(lastElements) / sizeof(lastElements[0]);

byte sendData = 1;
Timer sharedTimer;
Timer sharedAnimationTimer;
Timer goDelayTimer;
bool goBuffered = false;
byte spinnerOffset = 0;
byte goSignalRecievedFromFace = 0;

enum overallStateValues {OS_RESET_STATE, OS_PLAYER_STATE, OS_MASTER_STATE, OS_LEAF_STATE, OS_ALONE_STATE, OS_PIPE_STATE, OS_DEAD_STATE};
byte overallState = OS_PLAYER_STATE;
enum masterStateValues {MS_SETUP_STATE, MS_SPINNER_STATE, MS_SPOONS_STATE, MS_WINNER_STATE, MS_LOSER_STATE};
byte masterState = MS_SETUP_STATE;
enum pipeStateValues {PS_IDLE_STATE, PS_ANIM_STATE};
byte pipeState = PS_IDLE_STATE;
enum aloneStateValues {AS_IDLE_STATE, AS_ACTIVE_STATE};
byte aloneState = AS_IDLE_STATE;
enum leafStateValues {LS_IDLE_STATE, LS_ANIM_STATE};
byte leafState = LS_IDLE_STATE;


enum signalStates {INERT, GO, RESOLVE, PING};
byte signalState = INERT;
Timer goBroadcast; // Used to control how long you broadcast the go signal, hopefully this helps master pick up signals while it's polling both faces
const byte goBroadcastLength = 50;
bool runGoBroadcastTimer = false; // true if still waiting for delay to finish

Timer masterColorSwitchTimer;

void setup() {
  randomize();
}

void loop() {
  updateAdjacentMasters();
  updateSignalPropagation();
  updateMasterSetupState();
  switch (overallState) {
    case OS_RESET_STATE:
      osReset();
      break;
    case OS_PLAYER_STATE:
      osPlayer();
      break;
    case OS_MASTER_STATE:
      osMaster();
      break;
    case OS_LEAF_STATE:
      osLeaf();
      break;
    case OS_ALONE_STATE:
      osAlone();
      break;
    case OS_PIPE_STATE:
      osPipe();
      break;
    case OS_DEAD_STATE:
      osDead();
      break;
  }
}

void osReset() {
  if (sharedTimer.isExpired()) {
    overallState = OS_PLAYER_STATE;
  }
  if (currentPlayerRankCache < currentPlayerRankSignal) {
    currentPlayerRankCache = currentPlayerRankSignal;
  }
  if (currentPlayerRankCache == currentPlayerRankSignal && signalState == GO) {
    sharedTimer.set(resetStateLength);
  }
  setColor(MAGENTA);
}

void osDead() {
  setColor(RED);
  glitchRender(RED);
  glitchRender(RED);
}

// *****************************************************************
// ******* NON-MASTER HELPERS **************************************
// *****************************************************************


void osPlayer() {
  byte connectedFaces = 0;
  FOREACH_FACE(f) {
    if (shouldConsiderFace(f))
    {
      ++connectedFaces;
    }
  }
  if (connectedFaces == 0 && overallState != OS_ALONE_STATE) {
    overallState = OS_ALONE_STATE;
    aloneState = 0; // reset alone state machine
    sharedTimer.set(lonePieceActivationMin + random(lonePieceActivationMax - lonePieceActivationMin));
  }
  if (connectedFaces == 1 && overallState != OS_LEAF_STATE) {
    overallState = OS_LEAF_STATE;
    leafState = 0; // reset leaf state machine
    currentPlayerRankCache = 0;
    goBuffered = false;
    buttonPressed(); // reset button press checker before going into leaf state
  }
  if (connectedFaces > 1 && overallState != OS_PIPE_STATE) {
    overallState = OS_PIPE_STATE;
    currentPlayerRankCache = 0;
    pipeState = 0; // reset pipe state machine
  }
}

void pipeRender() {
  FOREACH_FACE(f) {
    if (shouldConsiderFace(f)) setColorOnFace(dim(WHITE, PIPE_BRIGHTNESS), f);
  }
}

void pipeAnimRender() {
  if (currentPlayerRankCache < currentPlayerRankSignal) {
    currentPlayerRankCache = currentPlayerRankSignal;
    sharedAnimationTimer.set(pipePropagationAnimationLength);
  }
  if (currentPlayerRankCache == currentPlayerRankSignal && signalState == GO) {
    sharedTimer.set(pipeDisplayLength);
  }
}

void glitchRender(Color color) {
  setColorOnFace(dim(color, 64), random(5));
}

// *****************************************************************
// ******* LEAF STATE **********************************************
// *****************************************************************


void osLeaf() {
  osPlayer();
  if (overallState != OS_LEAF_STATE) return;
  switch (leafState) {
    case LS_IDLE_STATE:
      lsIdle();
      break;
    case LS_ANIM_STATE:
      lsAnim();
      break;
  }
}

void lsIdle() {
  bool pressed = buttonPressed();
  if ((pressed && adjacentMasterFace == 6) || (goBuffered && goDelayTimer.isExpired())) {
    signalState = GO;
    currentPlayerRankSignal = RANK_NONE;
    goBuffered = false;
  }
  else if (pressed && !goBuffered && goDelayTimer.isExpired() && adjacentMasterFace < 6) {
    goDelayTimer.set(goDelay);
    goBuffered = true;
  }
  if (signalState == GO) {
    leafState = LS_ANIM_STATE;
    sharedTimer.set(pipeDisplayLength);
    return;
  }
  if (currentPlayerRankCache == RANK_NONE) {
    setColor(dim(GREEN, 128));
  }
  pipeRender();
  setColorOnFace(GREEN, random(5));
}

void lsAnim() {
  if (sharedTimer.isExpired()) {
    leafState = LS_IDLE_STATE;
    currentPlayerRankCache = 0;
    goBuffered = false;
    buttonPressed(); // reset button pressed state before going back to idle
    return;
  }
  pipeAnimRender();
  setColor(playerRankColors[currentPlayerRankCache]);
  glitchRender(playerRankColors[currentPlayerRankCache]);
}


// *****************************************************************
// ******* ALONE STATE *********************************************
// *****************************************************************

void osAlone() {
  osPlayer();
  if (overallState != OS_ALONE_STATE) return;
  switch (aloneState) {
    case AS_IDLE_STATE:
      asIdle();
      break;
    case AS_ACTIVE_STATE:
      asActive();
      break;
  }
}

void asIdle() {
  if (sharedTimer.isExpired()) {
    if (random(randomAloneDeathChance - 1) == 0) overallState = OS_DEAD_STATE;
    aloneState = AS_ACTIVE_STATE;
    return;
  }
  setColor(OFF);
  setColorOnFace(dim(YELLOW, 128), (sharedTimer.getRemaining()/90) % 6);
}

void asActive() {
  setColor(GREEN);
  glitchRender(GREEN);
  glitchRender(GREEN);
}

// *****************************************************************
// ******* PIPE STATE **********************************************
// *****************************************************************


void osPipe() {
  osPlayer();
  if (overallState != OS_PIPE_STATE) return;
  switch(pipeState) {
    case PS_IDLE_STATE:
      psIdle();
      break;
    case PS_ANIM_STATE:
      psAnim();
      break;
  }
}

void psIdle() {
  if (signalState == GO) {
    pipeState = PS_ANIM_STATE;
    sharedTimer.set(pipeDisplayLength);
    sharedAnimationTimer.set(pipePropagationAnimationLength);
    return;
  }
  currentPlayerRankCache = 0;
  setColor(OFF);
  pipeRender();
}

void psAnim() {
  if (sharedTimer.isExpired()) {
    pipeState = PS_IDLE_STATE;
    currentPlayerRankCache = 0;
    return;
  }
  pipeAnimRender();
  FOREACH_FACE(f) {
    if (shouldConsiderFace(f)) {
      setColorOnFace(playerRankColors[currentPlayerRankCache], f);
      if (f != goSignalRecievedFromFace && !sharedAnimationTimer.isExpired()) {
        setColorOnFace(dim(WHITE, 128), f);
      }
    }
  }
}

// *****************************************************************
// ******* MASTER STATE ********************************************
// *****************************************************************

void osMaster() {
  if (masterState != MS_SETUP_STATE) {
    setValueSentOnAllFaces((INERT << 1) + 1); // master state controls its own signal states
  }
  switch (masterState) {
    case MS_SETUP_STATE:
      msSetup();
      break;
    case MS_SPINNER_STATE:
      msSpinner();
      break;
    case MS_SPOONS_STATE:
      msSpoons();
      break;
    case MS_WINNER_STATE:
      msWinner();
      break;
    case MS_LOSER_STATE:
      msLoser();
      break;
  }
}

void msSetup() {
  if(sharedTimer.isExpired()) {
    masterState = MS_SPINNER_STATE;
    currentPlayerRankSignal = RANK_NONE;
    return;
  }
  if (sharedTimer.getRemaining() > masterSetupDontSendGoLength) {
    FOREACH_FACE(f) {
      if(!isValueReceivedOnFaceExpired(f) && getSignalState(f) != RESOLVE) {
        setValueSentOnFace((RANK_RESET << 3) + (GO << 1) + 1, f);
      }
      else {
        setValueSentOnFace((RESOLVE << 1) + 1, f);
      }
    }
  }
  setColor(BLUE);
}

void msSpinner() {
  currentPlayerRankCache = 0; // Reset rank cache used to track whether to send RANK_RESET
  if (masterColorSwitchTimer.isExpired()) {  // SET NEXT MASTER COLOR
      masterColorIndex = random(masterColorNum - 1);
      masterValue = masterValues[random(masterValuesNum - 1)];
      // Shift all stored combos in lastElements to the right. TODO put this in a function ?
      for(byte i=lastElementsNum-1; i>0; --i)
      {
          lastElements[i][0] = lastElements[i-1][0];
          lastElements[i][1] = lastElements[i-1][1];
      }
      lastElements[0][0] = masterColorIndex; // Overwrite first element with newest one
      lastElements[0][1] = masterValue;

      masterColorSwitchTimer.set(masterColorSwitchLength);

      if (phaseStepsTaken >= phaseDurations[phaseIndex]) {
          // After the first run through the phases, skip the first initial extra slow stage
          // Assuming 4 phases
          // 0 -> 1
          // 1 -> 2
          // 2 -> 3
          // 3 -> 1
          phaseIndex = (phaseIndex % (numPhases - 1)) + 1;
          phaseStepsTaken = 0;
      }

      // masterColorSwitchLength TODO maybe this variable can be removed
      masterColorSwitchLength += phaseDeltas[phaseIndex];
      phaseStepsTaken += 1;

      // masterColorSwitchLength -= masterColorSwitchLengthDelta;

      // masterColorSwitchLength += masterColorSwitchLengthDelta * masterColorSwitchLengthDirection;
      // if (masterColorSwitchLength < masterColorSwitchLengthMin || masterColorSwitchLength > masterColorSwitchLengthMax) {
      //     masterColorSwitchLengthDirection *= -1;
      // }

      if (masterColorSwitchLength < masterColorSwitchLengthMin) masterColorSwitchLength = masterColorSwitchLengthMin;
      spinnerOffset = random(5);
  } else if (masterColorSwitchTimer.getRemaining() < masterColorSwitchGapLength) { // Blink off for a bit
      displayCombo(dim(masterColors[masterColorIndex],
        masterColorSwitchTimer.getRemaining() * 255 /masterColorSwitchGapLength), masterValue);
      // displayCombo(masterColors[masterColorIndex], masterValue); // TODO Remove, need dimming on master or repeat combos are 
      // indistinguishable.
  } 
  else {
    displayCombo(masterColors[masterColorIndex], masterValue);
  }

  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)
        && getSignalState(f) == GO) {
      if (getColorState(f) == RANK_NONE) {
          // Received first player input

        // initialize ranking system
        masterNextRankToAssign = 1;
        numPlayersAtInputTime = 0;
        FOREACH_FACE(i) {
          playerRanks[i] = 7;
          if (!isValueReceivedOnFaceExpired(i)) {
            ++numPlayersAtInputTime;
            playerRanks[i] = 6;
          }
        }
        if (isValidPattern()) {
          playerRanks[f] = 0;
          masterState = MS_SPOONS_STATE;
          sharedTimer.set(winnerPendingWaitLength);
          resetStoredPattern();
        }
        else {
          playerRanks[f] = 5;
          masterState = MS_LOSER_STATE;
          sharedTimer.set(loserResultDisplayLength);
        }
        
        // reset pattern memory

        break;
      }
      else {
        setValueSentOnFace((RESOLVE << 1) + 1, f); // if blink is still sending GO from winner/loser phase
      }
    }
  }
}

void updateSpoonsSignals() {
  FOREACH_FACE(f) {
    byte sendVal = (GO << 1) + 1;
    if(!isValueReceivedOnFaceExpired(f) && getSignalState(f) != RESOLVE) {
      if (playerRanks[f] == 0) {
        sendVal = sendVal + (RANK_WIN << 3);
        setValueSentOnFace(sendVal, f);
      }
      else if (playerRanks[f] < numPlayersAtInputTime - 1) {
        sendVal = sendVal + (RANK_MID << 3);
        setValueSentOnFace(sendVal, f);
      }
      else if (playerRanks[f] >= numPlayersAtInputTime - 1 && playerRanks[f] < 6) {
        sendVal = sendVal + (RANK_LOSE << 3);
        setValueSentOnFace(sendVal, f);
      }
      else {
        setValueSentOnFace((INERT << 1) + 1, f);
      }
    }
    else {
      setValueSentOnFace((RESOLVE << 1) + 1, f);
    }
  }
}

void msSpoons() {
  byte lastPlayerRemaining = 6;
  byte playersRemaining = 0;
  if (!sharedTimer.isExpired()) {
    FOREACH_FACE(f) {
      if (playerRanks[f] == 6) {
        if (isValueReceivedOnFaceExpired(f) || getSignalState(f) == GO) { // Received next player input
            playerRanks[f] = masterNextRankToAssign++;
            sharedTimer.set(winnerPendingWaitLength);
        }
        else {
          ++playersRemaining;
          lastPlayerRemaining = f;
        }
      }
    }
  }
  else { // if nobody finishes the round after some time
    FOREACH_FACE(f) {
      if (playerRanks[f] == 6) {
        playerRanks[f] = 5;
      }
    }
  }

  if (playersRemaining == 1) {
    playerRanks[lastPlayerRemaining] = ++masterNextRankToAssign;
    --playersRemaining;
  }
  if (playersRemaining == 0) {
    buttonPressed();
    sharedTimer.set(masterResultDisplayLength); // TODO This technically isn't winner display timer because it also displays mistakes
    masterState = MS_WINNER_STATE;
  }
  updateSpoonsSignals();
}

void msWinner() {
  if (sharedTimer.isExpired() || buttonPressed()) {
    masterState = MS_SPINNER_STATE;
    return;
  }
  FOREACH_FACE(f) {
    //setColorOnFace(dim(masterColors[random(masterColorNum - 1)], sharedTimer.getRemaining() * 255 / masterResultDisplayLength), f);
    setColorOnFace(masterColors[random(masterColorNum - 1)], f);
  }
  updateSpoonsSignals();
}

void msLoser() {
  if (sharedTimer.isExpired()) {
    masterState = MS_SPINNER_STATE;
    return;
  }
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f) && getSignalState(f) == GO && playerRanks[f] == 6) {
      playerRanks[f] = 5;
      sharedTimer.set(masterResultDisplayLength);
      setValueSentOnFace((RESOLVE << 1) + 1, f);
    }
  }
  updateSpoonsSignals();
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

// Sets the stored pattern to 99 (init val)
void resetStoredPattern() {
    for (byte i=0; i<lastElementsNum; ++i) {
        lastElements[i][0] = 99;
        lastElements[i][1] = 99;
    }
}

void displayCombo(Color color, byte value) {
  setColor(OFF);
  if (value >= 1) {
      setColorOnFace(color, (0 + spinnerOffset) % 6);
  }
  if (value >= 3) {
      setColorOnFace(color, (2 + spinnerOffset) % 6);
  }
  if (value >= 6) {
      setColorOnFace(color, (4 + spinnerOffset) % 6);
  }
}

// *****************************************************************
// ******* SIGNAL PROPAGATION **************************************
// *****************************************************************

void updateSignalPropagation() {
  switch (signalState) {
    case INERT:
      inertLoop();
      break;
    case GO:
      if (runGoBroadcastTimer) { // Make sure timer runs only once
          goBroadcast.set(goBroadcastLength);
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
  sendData = (signalState << 1) + (currentPlayerRankSignal << 3);
  if (overallState == OS_MASTER_STATE) sendData += 1;
  setValueSentOnAllFaces(sendData);
}

// make color commands propagate regardless of state, reset when signal state is INERT
bool updatePlayerColor(byte f) {
  byte c = getColorState(f);
  if (c > currentPlayerRankSignal && !(overallState == OS_MASTER_STATE && masterState == MS_SETUP_STATE)) {
    currentPlayerRankSignal = c;
    goSignalRecievedFromFace = f;
  }
}

bool shouldConsiderFace(byte f) { //if adjacent to master, should I prop signals to this face?
  if (isValueReceivedOnFaceExpired(f)) return false;
  if (adjacentMasterFace == 6) return true;

  if (f == adjacentMasterFace
   || f == (adjacentMasterFace + 2) % 6
   || f == (adjacentMasterFace + 3) % 6
   || f == (adjacentMasterFace + 4) % 6) {
    return true;
  }
  return false;
}

void inertLoop() {
  // when inert, default win metadata to 0
  currentPlayerRankSignal = 0;

  //listen for neighbors in GO
  FOREACH_FACE(f) {
    if (shouldConsiderFace(f)) {//a neighbor!
      if (getSignalState(f) == GO) {//a neighbor saying GO!
        updatePlayerColor(f);
        signalState = GO;
        runGoBroadcastTimer = true;
        goSignalRecievedFromFace = f;
      }
    }
  }
}

// Confusing but saves 18 bytes
void goResolveLoopOptimization(byte a) {
  signalState = a; //I default to this at the start of the loop. Only if I see a problem does this not happen
  
  //look for neighbors who have not heard the GO news
  FOREACH_FACE(f) {
    if (shouldConsiderFace(f)) {//a neighbor!
      updatePlayerColor(f);
      if (getSignalState(f) == (a + 1) % 3) {//This neighbor doesn't know it's GO time. Stay in GO
        signalState = (a + 2) % 3;
      }
    }
  }
}

void goLoop() {
  goResolveLoopOptimization(RESOLVE);
}

void resolveLoop() {
  goResolveLoopOptimization(INERT);
}

void updateAdjacentMasters() {
  FOREACH_FACE(f) {
    // if connected and                          is adjacent master
    if (!isValueReceivedOnFaceExpired(f) && (getLastValueReceivedOnFace(f) & 1) == 1) {
      adjacentMasterFace = f;
    }
    //reset if signal is not from master when it is supposed to be
    else if (adjacentMasterFace == f) {
      adjacentMasterFace = 6;
    }
  }
}

void updateMasterSetupState() {
  if (overallState != OS_RESET_STATE
      && !(overallState == OS_MASTER_STATE && masterState == MS_SETUP_STATE)
      && currentPlayerRankSignal == RANK_RESET) {
    overallState = OS_RESET_STATE;
    signalState = GO;
    currentPlayerRankSignal = RANK_RESET;
    sharedTimer.set(resetStateLength);
  }
  // evaluate switched to master
  if (buttonLongPressed()) {
    overallState = OS_MASTER_STATE;
    masterState = MS_SETUP_STATE;
    signalState = GO; // Don't know if this is the best place for this TODO
    currentPlayerRankSignal = RANK_RESET;
    masterColorSwitchLength = masterColorSwitchLengthMax;
    sharedTimer.set(masterSetupStateLength);

    phaseIndex = 0;
    phaseStepsTaken = 0;
  }
}

byte getColorState(byte f) {
  return ((getLastValueReceivedOnFace(f) >> 3) & 7);//returns bits C and D
}

byte getSignalState(byte f) {
  return ((getLastValueReceivedOnFace(f) >> 1) & 3);//returns bits C and D
}
