import { ComponentFixture, TestBed } from '@angular/core/testing';

import { SourceTextWindowComponent } from './source-text-window.component';

describe('SourceTextWindowComponent', () => {
  let component: SourceTextWindowComponent;
  let fixture: ComponentFixture<SourceTextWindowComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ SourceTextWindowComponent ]
    })
    .compileComponents();

    fixture = TestBed.createComponent(SourceTextWindowComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
