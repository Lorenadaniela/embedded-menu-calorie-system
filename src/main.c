#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>


volatile int meniu = 1;        // MEN1, MEN2, MEN3 ,MEN4 ,MEN5, MEN6
volatile int meniu2 = 0;       // 0 = text (MENx), 1 = valoare numerica
volatile int editMode = 0;     // 0 = nu editam, 1 = editam valoarea numerica
volatile int digit = 0;        // Multiplexare afisaj


volatile int values[6] = {330, 210, 180, 150, 270, 315}; // MEN1, MEN2, MEN3 ,MEN4,MEN5,MEN6

volatile int potValue = 0;     // Valoare citita de la potentiometru
volatile int oldPotValue = -1; // Ultima valoare stabila
volatile int potDisplayTimer = 0; // 2 secunde pentru afisare potValue


void init_display(void)
{
	// PA0..PA3 pentru select digit, PC0..PC7 pentru segmente
	DDRA |= 0b00001111;
	DDRC |= 0b11111111; // Seteaza toti cei 8 pini ai PORTC ca iesire
	PORTA = 0b00000000; // Seteaza toti cei 8 pini ai PORTA la 0
	PORTC = 0b00000000; // Seteaza toti cei 8 pini ai PORTC la 0

}

void init_timer2(void)
{
	// Timer2 configurat pentru tick la 1 ms
	TCCR2 = 0b00001100; // CTC, prescaler 64
	OCR2 = 249; // 1 ms
	TIMSK |= 0b10000000;
}

void init_adc(void)
{
	
	ADMUX = 0b01000000; //AVCC
	ADCSRA = 0b10000111;//precaler 128
}


int readADC(uint8_t channel)
{
	// Selectam canalul
	ADMUX = (ADMUX & 0b11100000) | (channel & 0b00000111);
	ADCSRA |= (1 << ADSC); // Start conversie
	while (ADCSRA & (1 << ADSC));
	return ADC; // Returnam valoarea 0..1023
}


void init_buttons(void)
{
	// ENTER (INT0), NEXT (INT1), BACK (INT2)
	MCUCR |= 0b00000011;  // INT0 rising edge (ISC01 = 1, ISC00 = 1)
	MCUCR |= 0b00001100;  // INT1 rising edge (ISC11 = 1, ISC10 = 1)
	MCUCSR |= 0b01000000; // INT2 rising edge (ISC2 = 1)
	GICR  |= 0b11100000;  // Enable INT0, INT1, and INT2 (INT0 = 1, INT1 = 1, INT2 = 1)
}

void init_led(void)
{
	// LED pe PB7
	DDRB |= (1 << PB7);
	PORTB &= ~(1 << PB7); // LED OFF initial
}

void display(uint8_t digitIndex, int character)
{
	// Dezactivam toate digit-urile
	PORTA &= 0b11110000;
	PORTC = 0b00000000;


	// Segmente
	if (character >= 0 && character <= 9)
	{
		switch (character)
		{
			case 0: PORTC = 0b00111111; break;
			case 1: PORTC = 0b00000110; break;
			case 2: PORTC = 0b01011011; break;
			case 3: PORTC = 0b01001111; break;
			case 4: PORTC = 0b01100110; break;
			case 5: PORTC = 0b01101101; break;
			case 6: PORTC = 0b01111101; break;
			case 7: PORTC = 0b00000111; break;
			case 8: PORTC = 0b01111111; break;
			case 9: PORTC = 0b01101111; break;
		}
	}
	else
	{
		switch (character)
		{
			case 'M': PORTC = 0b00110111; break;
			case 'E': PORTC = 0b01111001; break;
			case 'N': PORTC = 0b01110110; break;
			default:  PORTC = 0b00000000; break;
		}
	}

	// Activam digitul corespunzator
	switch (digitIndex)
	{
		case 1: PORTA |= (1 << PA3); break;
		case 2: PORTA |= (1 << PA2); break;
		case 3: PORTA |= (1 << PA1); break;
		case 4: PORTA |= (1 << PA0); break;
	}
}


ISR(TIMER2_COMP_vect)
{
	digit++;
	if (digit > 4) digit = 1;

	// Citire pot la fiecare 50 ms
	static uint8_t adcMs = 0;
	adcMs++;
	if (adcMs >= 50)
	{
		adcMs = 0;
		int val = readADC(6);

		// Prag de 5% pentru diferenta
		int threshold = 1023 / 20; // 5% din 1023
		if (abs(val - oldPotValue) > threshold)
		{
			potValue = val;
			oldPotValue = val;
			potDisplayTimer = 2000; // 2 secunde
		}

		// Control LED: aprindem daca oldPotValue > valoarea curenta a meniului
		if (oldPotValue > values[meniu - 1])
		PORTB |= (1 << PB7); // LED ON
		else
		PORTB &= ~(1 << PB7); // LED OFF
	}

	// Cronometru pentru afisare potValue
	if (potDisplayTimer > 0)
	{
		potDisplayTimer--;
	}

	// Afisare pe 7-segmentdisplay
	if (potDisplayTimer > 0)
	{
		int v;
		if (potValue > 999) {
			v = 999;
			} else {
			v = potValue;
		}
		int s0 = v % 10;
		int s1 = (v / 10) % 10;
		int s2 = (v / 100) % 10;
		int s3 = (v / 1000) % 10;
		switch (digit)
		{
			case 1: display(4, s3); break;
			case 2: display(3, s2); break;
			case 3: display(2, s1); break;
			case 4: display(1, s0);  break;
		}
	}
	else
	{
		if (meniu2 == 0)
		{
			switch (digit)
			{
				case 1: display(4, 'M'); break;
				case 2: display(3, 'E'); break;
				case 3: display(2, 'N'); break;
				case 4: display(1, meniu); break;
			}
		}
		else
		{
			int v = values[meniu - 1];
			int s0 = v % 10;
			int s1 = (v / 10) % 10;
			int s2 = (v / 100) % 10;
			int s3 = (v / 1000) % 10;


			switch (digit)
			{
				case 1: display(4, s3); break;
				case 2: display(3, s2); break;
				case 3: display(2, s1); break;
				case 4: display(1, s0); break;
			}
		}
	}
}

// PD2
ISR(INT0_vect)
{
	if (meniu2 == 0)
	{
		meniu2 = 1; // Trecem la afisarea valorii numerice
	}
	else if (editMode == 0)
	{
		editMode = 1; // Activam editarea valorii
	}
	else
	{
		editMode = 0; // Iesim din editare
	}
}

// PD3
ISR(INT1_vect)
{
	if (editMode == 0 && meniu2 == 0)
	{
		meniu++;
		if (meniu > 6) meniu = 1; // Rotim �ntre MEN1, MEN2, MEN3
	}
	else if (editMode == 1)
	{
		values[meniu - 1]++;
		
		if (values[meniu - 1] > 999)
		
		values[meniu - 1] = 0; // Limitam valoarea
	}
}

// PB2
ISR(INT2_vect)
{
	if (editMode == 1)
	{
		editMode = 0; // Iesim din editare
	}
	else if (meniu2 == 1)
	{
		meniu2 = 0; // Revenim la meniul principal (textul "MENx")
	}
}


int main(void)
{
	init_display();
	init_timer2();
	init_adc();
	init_buttons();
	init_led();
	sei(); // Activam �ntreruperile globale

	while (1)
	{
		// Totul se �nt�mpla �n�ISR-uri
	}

}
