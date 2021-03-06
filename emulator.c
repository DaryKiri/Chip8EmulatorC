#include "emulator.h"

// Initializes the state of the cpu
State * InitChip8()
{
	State * chip8State = calloc(1, sizeof(State));
	chip8State->memory = malloc(1024*4);
	// Initialize memory
	memset(chip8State->memory,0,1024*4);
	// Initialize screen array
	//memset(chip8State->screen,0,sizeof(uint8_t)*64*32); //Or in sizeof use uint8_t
	int i = 0;
	int j;
	for(; i < 32; ++i)
	{
		for(; j < 64; ++j)
		{
			chip8State->screen[i][j]= 0;
		}
	}
	
	//chip8State->screen = &chip8State->memory[SCREEN_BASE]; 
	memset(chip8State->Stack,0,sizeof(uint16_t)*16);
	chip8State->SP = -1;
	chip8State->PC = 0x200;
	chip8State->I = 0x0;
	chip8State->DT = 0x0;
	chip8State->ST = 0x0;
	chip8State->AdvancePC = 0x01;
	chip8State->waitKey = 0x00;
	chip8State->drawFlag = 0x0;
	
	/*
	int i;
	for(i=0; i < 16; ++i)
	{
		chip8State->V[i] = 0x0;
		chip8State->keys[i] = 0x0;
	}
	*/
	
	// Initialize registers
	memset(chip8State->V,0,sizeof(uint8_t)*16);
	// Initialize key states
	memset(chip8State->keys,0,sizeof(uint8_t)*16);
	
	//Initialize fonts
	uint8_t fonts[] = {
		//0
		0xF0, 0x90, 0X90, 0X90, 0XF0,	
		//1
		0x20, 0x60, 0x20, 0x20, 0x70,
		//2
		0xF0, 0x10, 0xF0, 0x80, 0xF0,
		//3
		0xF0, 0x10, 0xF0, 0x10, 0xF0,
		//4
		0x90, 0x90, 0xF0, 0x10, 0x10,
		//5
		0xF0, 0x80, 0xF0, 0x10, 0xF0,
		//6
		0xF0, 0x80, 0xF0, 0x90, 0xF0,
		//7
		0xF0, 0x10, 0x20, 0x40, 0x40,
		//8
		0xF0, 0x90, 0xF0, 0x90, 0xF0,
		//9
		0xF0, 0x90, 0xF0, 0x10, 0xF0,
		//A
		0xF0, 0x90, 0xF0, 0x90, 0x90,
		//B
		0xE0, 0x90, 0xE0, 0x90, 0xE0,
		//C
		0xF0, 0x80, 0x80, 0x80, 0xF0,
		//D
		0xE0, 0x90, 0x90, 0x90, 0xE0,
		//E
		0xF0, 0x80, 0xF0, 0x80, 0xF0,
		//F
		0xF0, 0x80, 0xF0, 0x80, 0x80,
	};
	memcpy(chip8State->memory,fonts,FONT_SIZE);
	
	srand(time(NULL)); //For RND instruction

	return chip8State;
}

//Load a room
void LoadRoom(State * state, char * path)
{
	FILE * file;
	long size;
	
	if(NULL == (file = fopen(path,"rb")))
	{
		fprintf(stderr,"Error loading the rom\n");
		exit(1);
	}
	
	//Get size of the file in bytes
	if(-1 == (fseek(file, 0L, SEEK_END)))
	{
		fprintf(stderr,"Error setting file position to end\n");
		exit(1);
	}
	size = ftell(file);
	if(-1 == (fseek(file, 0L, SEEK_SET)))
	{
		fprintf(stderr,"Error setting file position to begining\n");
		exit(1);
	}
	
	//Read the file and store it on the memory of the chip-8
	if(0 == (fread(&state->memory[state->PC], size, 1, file)))
	{
		if(ferror(file))
		{
			fprintf(stderr,"Error reading the file\n");
			exit(1);
		}
		else
		{
			fprintf(stderr,"End of file reached, cant read more\n");
			exit(1);
		}
	}
	
	if( 0 != fclose(file))
	{
		fprintf(stderr,"Error upon closing file stream\n");
		exit(1);
	}
}

// Decodes instruction and prints to the console for debugging
void Decode(uint8_t * code, uint16_t pc, Instruction * inst)
{
	inst->firstByte = code[pc];
	inst->secondByte = code[pc+1];
	inst->firstNib = (code[pc] >> 4);
	inst->secondNib = (code[pc] & 0x0f);
	inst->finalNib = (code[pc+1] & 0x0f);
}

// Exit emulator
void ExitEmu(State * state, Instruction * inst, SDL_Window * eWindow, SDL_Renderer * eRenderer)
{
	//Close display
	SDL_DestroyRenderer(eRenderer);
	SDL_DestroyWindow(eWindow);
	//IMG_Quit();
	SDL_Quit();
	
	//Destroy structs
	free(state);
	free(inst);
}

//Refresh Timers
void RefreshTimer(State * state)
{
	if(state->DT > 0)
		state->DT -= 1;
	
	if(state->ST > 0)
		state->ST -= 1;
}

// Advance pc
void Advance(State * state)
{
	state->PC += 2;
}

//	Initialize the display 
void InitDisplay(SDL_Window ** eWindow, SDL_Renderer ** eRenderer)
{
	if(SDL_Init(SDL_INIT_VIDEO) != 0)
	{
		fprintf(stderr,"Error on initializing display: %s\n",SDL_GetError());
		exit(1); 
	}
	
	*eWindow = SDL_CreateWindow("Chip-8 Emulator",
								SDL_WINDOWPOS_UNDEFINED,
								SDL_WINDOWPOS_UNDEFINED,
								DISPLAY_WIDHT,
								DISPLAY_HEIGHT,
								0);
	
	if( NULL == *eWindow)
	{
		fprintf(stderr,"Error creating main window: %s\n",SDL_GetError());
		exit(1);
	}
	
	// Create renderer
	if(NULL == (*eRenderer = SDL_CreateRenderer(*eWindow, -1, SDL_RENDERER_SOFTWARE)))
	{
		fprintf(stderr,"Error creating renderer: %s\n",SDL_GetError());
		exit(1);
	}
	
	// Initialize renderer color
	if(SDL_SetRenderDrawColor(*eRenderer, BLACK, BLACK, BLACK, OPAQUE) < 0) // Block color with no alpha
	{
		fprintf(stderr,"Error setting renderer draw color: %s\n",SDL_GetError());
		exit(1);
	}

	// Clear Renderer
	SDL_RenderClear(*eRenderer);

	/*
	//Initialize SDL_image library
	int imgflags = IMG_INIT_PNG | IMG_INIT_JPG;
	
	if( !(IMG_Init(imgflags) & imgflags))
	{
		fprintf(stderr, "Error loading SDL_image library: %s\n",IMG_GetError());
		exit(1);
	}
	*/
	
}

// Refresh Display
void UpdateDisplay(SDL_Renderer * eRenderer, State * state)
{
	SDL_Rect rectangle;
	
	int i = 0;
	int j;
	for(; i < 32; ++i)
	{
		j = 0;
		for(; j < 64; ++j)
		{
			// Set rectangle to render on screen
			rectangle.x = j*10;
			rectangle.y = i*10;
			rectangle.w = 10;
			rectangle.h = 10;
			// Draw the rectangle on screen
			if(state->screen[i][j]) // 1 == White
			{
				SDL_SetRenderDrawColor(eRenderer, WHITE, WHITE, WHITE, OPAQUE);
			}
			else // 0 == Black
			{
				SDL_SetRenderDrawColor(eRenderer, BLACK, BLACK, BLACK, OPAQUE);
			}
			SDL_RenderFillRect(eRenderer, &rectangle);
		}
	}
	SDL_RenderPresent(eRenderer);
}

// Process Input
void ProcessInput(State * state)
{
	const uint8_t * keyStates = SDL_GetKeyboardState(NULL);

	// Clear array tsate
	memset(state->keys, 0, sizeof(uint8_t) * 16);

	// Update state of keys, form more information about keybinds, check emulator.h
	if (keyStates[SDL_SCANCODE_B])
	{
		state->keys[0x00] = 0x01;
	}
	if (keyStates[SDL_SCANCODE_4])
	{
		state->keys[0x01] = 0x01;
	}
	if (keyStates[SDL_SCANCODE_5])
	{
		state->keys[0x02] = 0x01;
	}
	if (keyStates[SDL_SCANCODE_6])
	{
		state->keys[0x03] = 0x01;
	}
	if (keyStates[SDL_SCANCODE_R])
	{
		state->keys[0x04] = 0x01;
	}
	if (keyStates[SDL_SCANCODE_T])
	{
		state->keys[0x05] = 0x01;
	}
	if (keyStates[SDL_SCANCODE_Y])
	{
		state->keys[0x06] = 0x01;
	}
	if (keyStates[SDL_SCANCODE_F])
	{
		state->keys[0x07] = 0x01;
	}
	if (keyStates[SDL_SCANCODE_G])
	{
		state->keys[0x08] = 0x01;
	}
	if (keyStates[SDL_SCANCODE_H])
	{
		state->keys[0x09] = 0x01;
	}
	if (keyStates[SDL_SCANCODE_V])
	{
		state->keys[0x0A] = 0x01;
	}
	if (keyStates[SDL_SCANCODE_N])
	{
		state->keys[0x0B] = 0x01;
	}
	if (keyStates[SDL_SCANCODE_7])
	{
		state->keys[0x0C] = 0x01;
	}
	if (keyStates[SDL_SCANCODE_U])
	{
		state->keys[0x0D] = 0x01;
	}
	if (keyStates[SDL_SCANCODE_J])
	{
		state->keys[0x0E] = 0x01;
	}
	if (keyStates[SDL_SCANCODE_M])
	{
		state->keys[0x0F] = 0x01;
	}

}

//	Instrucion implementations
void JumpCallReturn(State * state, Instruction inst) // SYS, JP, CALL, RET
{
	uint16_t dir; // for 0x0B instruction
	switch(inst.firstNib)
	{
		case 0x00:
			// RET: sets PC to the adress on top of the stack and decrements the stack pointer
			if(inst.firstByte == 0x00 && inst.secondByte == 0xEE) 
			{
				if(state->SP > -1)
				{
					state->PC = state->Stack[state->SP];
					state->SP -= 1;
				}
				else
				{
					fprintf(stderr,"Error pop on stack, stack empty and SP = %d\n",state->SP);
					exit(1);
				} 
			}
			// SYS: jump to a machine code routine at nnn
			else
			{
				state->PC = (inst.firstByte << 8) | inst.secondByte;
				state->AdvancePC = 0;
			}
			break;
		case 0x01:
			// JP: jump to location nnn
			state->PC = (0x0F00 & (inst.firstByte << 8)) | inst.secondByte;
			state->AdvancePC = 0;
			break;
		case 0x02:
			// CALL: call subroutine at nnn, increments stack pointer then put PC on top of stack
			if(state->SP > -2 && state->SP < 16)
			{
				state->SP += 1;
				state->Stack[state->SP] = state->PC+2;
				state->PC = ((inst.firstByte & 0x00F) << 8) | inst.secondByte;
			}
			else
			{
				fprintf(stderr, "Error on push, stack full, state->SP = %d\n",state->SP);
				exit(1);
			}
			state->AdvancePC = 0;
			break;
		case 0x0B:
			// JP: jump to location nnn + V0
			dir = (0x0000 | inst.secondNib) << 8;
			dir = dir | inst.secondByte;
			state->PC = dir + state->V[0];
			state->AdvancePC = 0;
			break;
		default:
			fprintf(stderr,"Error, instruction not avalible\n");
			exit(1);
	}
}

void ClearScreen(State * state, Instruction inst) // CLS
{
	//memset(state->screen,0,SCREEN_SIZE);
	memset(state->screen,0,sizeof(state->screen[0][0])*64*32);
	// Set all to black
	//SDL_SetRenderDrawColor(eRenderer, BLACK, BLACK, BLACK, OPAQUE);
	//SDL_RenderClear(eRenderer);
	// Set draw flag
	state->drawFlag = 0x01;
}

void SkipIfEqualIn(State * state, Instruction inst) // SE Vx, nn [Skip if Vx == nn]
{
	if(state->V[inst.secondNib] == inst.secondByte)
	{
		state->PC += 2;
	}
}

void SkipIfNotEqualIn(State * state, Instruction inst) // SNE Vx, nn [Skip if Vx != nn]
{
	if(state->V[inst.secondNib] != inst.secondByte)
	{
		state->PC += 2;
	}	
}

void SkipIfEqualReg(State * state, Instruction inst) // SE Vx, Vy [Skip if Vx == Vy]
{
	if(state->V[inst.secondNib] == state->V[inst.secondByte >> 4])
	{
		state->PC += 2;
	}
}

void SkipIfNotEqualReg(State * state, Instruction inst) // SNE Vx, Vy [Skip if Vx != Vy]
{
	if(state->V[inst.secondNib] != state->V[inst.secondByte >> 4])
	{
		state->PC += 2;
	}	
}

void LoadValue(State * state, Instruction inst) // LD Vx, nn
{
	state->V[inst.secondNib] = inst.secondByte;
}

void AddIn(State * state, Instruction inst) // ADD Vx, nn
{
	state->V[inst.secondNib] += inst.secondByte;
}

void Move(State * state, Instruction inst) // LD Vx, Vy
{
	// Stores value of register Vy to register Vx
	state->V[inst.secondNib] = state->V[inst.secondByte >> 4];
}

void Arithmetic(State * state, Instruction inst) // LD [Vx], [Vy], OR, AND, XOR, ADD, SUB, SHR, SHL, SUBN 
{
	switch(inst.finalNib)
	{
		case 0x00:
			Move(state, inst);
			break;
		case 0x01:
			//Bitwise OR: Vx OR Vy
			state->V[inst.secondNib] = state->V[inst.secondNib] | state->V[inst.secondByte >> 4]; 
			break;
		case 0x02:
			//Bitwise AND: Vx AND Vy
			state->V[inst.secondNib] = state->V[inst.secondNib] & state->V[inst.secondByte >> 4]; 
			break;
		case 0x03:
			//Bitwise XOR: Vx XOR Vy
			state->V[inst.secondNib] = state->V[inst.secondNib] ^ state->V[inst.secondByte >> 4]; 
			break;
		case 0x04: //ADD
			// Set Vx = Vx + Vy, if result is bigger than 8 bits (result > 255) VF is set to 1 (carry)
			if(0xFF < state->V[inst.secondNib] + state->V[inst.secondByte >> 4])
			{
				state->V[0x0F] = 0x01;
			}
			state->V[inst.secondNib] = state->V[inst.secondNib] + state->V[inst.secondByte >> 4];  
			break;
		case 0x05: // SUB
			// Set Vx = Vx - Vy if Vx > Vy then VF is set to 1 (not borrow), otherwise 0. Then Vy is subtracted from Vx and stored in Vx 
			if(state->V[inst.secondNib] > state->V[inst.secondByte >> 4])
			{
				state->V[0x0F] = 0x01; // not borrow
			}
			state->V[inst.secondNib] = state->V[inst.secondNib] - state->V[inst.secondByte >> 4];  
			break;
		case 0x06: // SHR
			// Set Vx = Vx Right Shift 1 if the least-significant bit of Vx is 1, then VF is set to 1, otherwise 0
			if(0x01 & state->V[inst.secondNib])
			{
				state->V[0x0F] = 0x01;
			}
			state->V[inst.secondNib] = state->V[inst.secondNib] >> 1;
			break;
		case 0x07: // SUBN
			// Set Vx = Vy - Vx if Vy > Vx then VF is set to 1 (not borrow), otherwise 0. Then Vx is subtracted from Vy and stored in Vx
			if(state->V[inst.secondByte >> 4] > state->V[inst.secondNib])
			{
				state->V[0x0F] = 0x01; // not borrow
			}
			state->V[inst.secondNib] = state->V[inst.secondByte >> 4] - state->V[inst.secondNib];  
			break;
		case 0x0E: // SHL
			//Set Vx = Vx Left Shift 1 if most-significant bit of Vx is 1, then VF is set to 1, otherwise 0
			if(0x08 & state->V[inst.secondNib])
			{
				state->V[0x0F] = 0x01;
			}
			state->V[inst.secondNib] = state->V[inst.secondNib] << 1;
			break; 
		default:
			fprintf(stderr, "Error aritmethic instrucion not avalible: %02x\n",inst.finalNib);
			break;
	}
}

void SetAddress(State * state, Instruction inst) // LD I, nnn [I = nnn]
{
	//Stores inmediate value nnn to the register I (memory register)
	uint16_t dir = (0x0000 | inst.secondNib) << 8;
	dir = dir | inst.secondByte; 
	state->I = dir;
}

void Random(State * state, Instruction inst) // RND Vx, nn
{
	uint8_t n = rand() % 256; // [0-255]
	state->V[inst.secondNib] = n & inst.secondByte;
}

void Draw(State * state, Instruction inst) // DRW Vx, Vy, n(nibble) 
{	// Draws an sprite of 8 pixels wide and n pixels high (max 16 pixel high) and set VF to 0 or 1
	// TO-DO
	int x = (int) state->V[inst.secondNib];
	int y = (int) state->V[inst.secondByte >> 4];
	int n = (int) inst.finalNib;
	uint8_t byte;
	
	state->V[0x0F] = 0x00;
	
	int i = 0;
	int j;
	for(; i < n; ++i) // y pos
	{
		j = 0;
		for(;j < 8; ++j) // x pos
		{
			byte =  (state->memory[state->I+i] >> (8 - (j+1))) & 0x01;
			if(byte != state->screen[y+i][x+j]) // Pixel change, set register VF, draw and update screen array
			{	
				state->V[15] = 1; // Set VF flag
				state->screen[y+i][x+j] = byte; // Update screen array
			}
		}
	}
	
	state->drawFlag = 0x01;
}

void SkipIfKeyPress(State * state, Instruction inst) // SKP Vx
{
	if(state->keys[state->V[inst.secondNib]])
	{
		state->PC += 2;
	} 
}

void SkipIfKeNotPress(State * state, Instruction inst) // SKNP Vx
{
	if(!state->keys[state->V[inst.secondNib]])
	{
		state->PC += 2;
	}	
}

void MiscInstruction(State * state, Instruction inst) // 0x0F instructions
{
	//TO-DO
	uint8_t ones, tens, hundreds, number; // for 0x33 instruction
	uint8_t i;
	switch(inst.secondByte)
	{
		case 0x07:
			//Set Vx = DT, load the value of the delay timer on register Vx
			state->V[inst.secondNib] = state->DT;
			break;
		case 0x0A:
			// Wait for a key press and store value of the key in Vx, execution stops until a key is pressed
			//TO-DO
			if(state->waitKey)
			{
				uint8_t i = 0x00;
				int quit = 0;
				while (i < 0x10 && !quit) // 16 buttons
				{
					if (state->keys[i])
					{
						state->V[inst.secondNib] = i;
						quit = 1;
					}
				}
				if (quit)
				{
					state->waitKey = 0x00;
				}
			}
			else
			{
				state->waitKey = 0x01; // On main, dont advance PC
			}
			break;
		case 0x15:
			//Set DT = Vx, DT is set equal to the value of Vx
			state->DT = 0x00;
			state->DT = state->V[inst.secondNib];
			break;
		case 0x18:
			// Set sound timer = Vx, ST is set equal to the value of Vx
			state->ST = 0x00;
			state->ST = state->V[inst.secondNib];
			break;
		case 0x1E:
			// Set I = I + Vx, values of I and Vx are added and stored in I
			state->I = state->I + state->V[inst.secondNib];
			break;
		case 0x29:
			//Set I = location of sprite for digit Vx, value of I is set to the location for the hexadecimal 
			//sprite corresponding to the value of Vx, and sprite of letters [0-F]
			state->I = (state->V[inst.secondNib]*5); //Because letters are stored from 0x000 to 0x06E
			break;
		case 0x33:
			//Store BCD representation of Vx in memory locations I(hundreds digit), I+1 (tens digit), I+2 (ones digit). 
			number = state->V[inst.secondNib];
			ones = number % 10;
			number = number / 10;
			tens = number % 10;
			hundreds = number / 10;
			state->memory[state->I] = hundreds;
			state->memory[state->I+1] = tens;
			state->memory[state->I+2] = ones; 
			break;
		case 0x55:
			//Store registers V0 trough Vx in memory starting at location I
			i = 0x00;
			while(i <= state->V[inst.secondNib])
			{
				state->memory[state->I+i] = state->V[i];
			}
			break;
		case 0x65:
			//Read registers V0 through Vx from memory starting at location I
			i = 0;
			while(i < state->V[inst.secondNib])
			{
				 state->V[i] = state->memory[state->I+i];
				 ++i;
			}
			break;
		default:
			fprintf(stderr,"Error, unavalible miscelaneous instruction: %02x\n",inst.secondByte);
			exit(1);
	}
}

// Executes the current instruction
void Execute(State * state, Instruction inst)
{
	state->AdvancePC = 0x01; // Reset advance pc state
	state->drawFlag = 0x00; // Reset Draw Flag
	
	switch(inst.firstNib)
	{
		case 0x00:
			if(inst.secondByte == 0xE0)
			{
				ClearScreen(state,inst);
			}
			else if(inst.firstByte == 0x00 && inst.secondByte == 0x00) 
			{
				//NOOP Instruction
			}else if(inst.secondByte == 0xEE)
			{
				JumpCallReturn(state,inst);
			}
			else
			{
				fprintf(stderr,"Error, unreconized instruction %02x %02x, PC: %04x\n",inst.firstByte, inst.secondByte, state->PC);
				exit(1);
			}
			break;
		case 0x01:
			JumpCallReturn(state,inst);
			break;
		case 0x02:
			JumpCallReturn(state,inst);
			break;
		case 0x03:
			SkipIfEqualIn(state,inst);
			break;
		case 0x04:
			SkipIfNotEqualIn(state,inst);
			break;
		case 0x05:
			SkipIfEqualReg(state,inst);
			break;
		case 0x06:
			LoadValue(state,inst);
			break;
		case 0x07:
			AddIn(state,inst);
			break;
		case 0x08:
			Arithmetic(state,inst);
			break;
		case 0x09:
			SkipIfNotEqualReg(state,inst);
			break;
		case 0x0A:
			SetAddress(state,inst);
			break;
		case 0x0B:
			JumpCallReturn(state,inst);
			break;
		case 0x0C:
			Random(state, inst);
			break;
		case 0x0D:
			Draw(state, inst);
			break;
		case 0x0E:
			if(inst.secondByte == 0x9E)
			{
				SkipIfKeyPress(state,inst);
			}
			else if(inst.secondByte == 0xA1)
			{
				SkipIfKeNotPress(state,inst);
			}
			else
			{
				fprintf(stderr,"Error, instruction not avalible: %02x %02x\n",inst.firstByte, inst.secondByte);
				exit(1);
			}
			break;
		case 0x0F:
			MiscInstruction(state, inst);
			break;
		default:
			fprintf(stderr,"Error, instruction not avalible: %01x\n",inst.firstNib);
			exit(1);
	}
	
	if(state->ST > 0)
	{
		printf("BEEP!\n");
		fflush(stdout);
	}
}

